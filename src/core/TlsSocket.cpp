#include "TlsSocket.h"

#include <windows.h>
#include <schannel.h>
#include <wincrypt.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")

using namespace std;

namespace {

// [GROUP: Constants]
constexpr DWORD kHandshakeRecvChunk = 16 * 1024;

// [GROUP: Internal Helpers]
wstring Utf8ToWide(const string& value) {
    if (value.empty()) {
        return wstring();
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 1) {
        return wstring();
    }

    wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], required);
    result.pop_back();
    return result;
}

// [GROUP: Internal Helpers]
string StatusToHex(SECURITY_STATUS status) {
    stringstream stream;
    stream << "0x" << hex << uppercase << static_cast<unsigned long>(status);
    return stream.str();
}

// [GROUP: Internal Helpers]
bool ReadBinaryFile(const string& path, vector<BYTE>& bytes) {
    ifstream input(path, ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(0, ios::end);
    const streamoff size = input.tellg();
    if (size <= 0) {
        return false;
    }

    input.seekg(0, ios::beg);
    bytes.resize(static_cast<size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    return input.good();
}

}  // namespace

// [GROUP: Lifecycle]
TlsSocket::TlsSocket()
    : socketHandle_(INVALID_SOCKET),
      credentialsReady_(false),
      contextReady_(false),
      streamSizesReady_(false),
      cbHeader_(0),
      cbTrailer_(0),
      cbMaximumMessage_(0),
      serverCertificateContext_(nullptr),
      decryptedOffset_(0),
      encryptedDataLen_(0) {
    memset(&credHandle_, 0, sizeof(credHandle_));
    memset(&ctxtHandle_, 0, sizeof(ctxtHandle_));
}

// [GROUP: Lifecycle]
TlsSocket::~TlsSocket() {
    Shutdown();
}

// [GROUP: TLS Setup]
bool TlsSocket::AcquireClientCredentials(bool insecureSkipVerify, string& error) {
    SCHANNEL_CRED credData{};
    credData.dwVersion = SCHANNEL_CRED_VERSION;
    credData.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
    credData.dwFlags = SCH_USE_STRONG_CRYPTO;
    if (insecureSkipVerify) {
        credData.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
    } else {
        credData.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION;
    }

    TimeStamp expiry{};
    const SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr,
        const_cast<SEC_WCHAR*>(UNISP_NAME_W),
        SECPKG_CRED_OUTBOUND,
        nullptr,
        &credData,
        nullptr,
        nullptr,
        &credHandle_,
        &expiry);

    if (status != SEC_E_OK) {
        error = "AcquireCredentialsHandle(client) failed: " + StatusToHex(status);
        return false;
    }

    credentialsReady_ = true;
    return true;
}

// [GROUP: TLS Setup]
bool TlsSocket::AcquireServerCredentials(const string& pfxPath,
                                         const string& pfxPassword,
                                         string& error) {
    if (pfxPath.empty()) {
        error = "TLS certificate path is empty. Set APP_TLS_CERT_PFX_PATH.";
        return false;
    }

    vector<BYTE> pfxBytes;
    if (!ReadBinaryFile(pfxPath, pfxBytes)) {
        error = "Failed to read PFX file: " + pfxPath;
        return false;
    }

    CRYPT_DATA_BLOB pfxBlob{};
    pfxBlob.cbData = static_cast<DWORD>(pfxBytes.size());
    pfxBlob.pbData = pfxBytes.data();

    const wstring pfxPasswordWide = Utf8ToWide(pfxPassword);
    HCERTSTORE certStore = PFXImportCertStore(
        &pfxBlob,
        pfxPasswordWide.empty() ? nullptr : pfxPasswordWide.c_str(),
        CRYPT_USER_KEYSET | CRYPT_EXPORTABLE);
    if (certStore == nullptr) {
        error = "PFXImportCertStore failed.";
        return false;
    }

    PCCERT_CONTEXT certContext = CertFindCertificateInStore(
        certStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_ANY,
        nullptr,
        nullptr);

    if (certContext == nullptr) {
        CertCloseStore(certStore, 0);
        error = "No certificate found in PFX file.";
        return false;
    }

    serverCertificateContext_ = CertDuplicateCertificateContext(certContext);
    CertFreeCertificateContext(certContext);
    CertCloseStore(certStore, 0);

    SCHANNEL_CRED credData{};
    credData.dwVersion = SCHANNEL_CRED_VERSION;
    credData.cCreds = 1;
    credData.paCred = &serverCertificateContext_;
    credData.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
    credData.dwFlags = SCH_USE_STRONG_CRYPTO;

    TimeStamp expiry{};
    const SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr,
        const_cast<SEC_WCHAR*>(UNISP_NAME_W),
        SECPKG_CRED_INBOUND,
        nullptr,
        &credData,
        nullptr,
        nullptr,
        &credHandle_,
        &expiry);

    if (status != SEC_E_OK) {
        error = "AcquireCredentialsHandle(server) failed: " + StatusToHex(status);
        return false;
    }

    credentialsReady_ = true;
    return true;
}

// [GROUP: Internal Transport Helpers]
bool TlsSocket::ReadFromSocket() {
    if (socketHandle_ == INVALID_SOCKET) {
        return false;
    }

    if (encryptedBuffer_.size() < encryptedDataLen_ + kHandshakeRecvChunk) {
        encryptedBuffer_.resize(encryptedDataLen_ + kHandshakeRecvChunk);
    }

    const int received = recv(
        socketHandle_,
        encryptedBuffer_.data() + encryptedDataLen_,
        static_cast<int>(encryptedBuffer_.size() - encryptedDataLen_),
        0);

    if (received <= 0) {
        return false;
    }

    encryptedDataLen_ += static_cast<size_t>(received);
    return true;
}

// [GROUP: TLS Setup]
bool TlsSocket::PerformClientHandshake(const string& serverName, string& error) {
    DWORD contextFlags = ISC_REQ_SEQUENCE_DETECT |
                         ISC_REQ_REPLAY_DETECT |
                         ISC_REQ_CONFIDENTIALITY |
                         ISC_REQ_ALLOCATE_MEMORY |
                         ISC_REQ_STREAM;

    DWORD outFlags = 0;
    TimeStamp expiry{};

    SecBuffer outBuffer{};
    outBuffer.BufferType = SECBUFFER_TOKEN;
    SecBufferDesc outBufferDesc{};
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = &outBuffer;
    outBufferDesc.ulVersion = SECBUFFER_VERSION;

    const wstring targetNameWide = Utf8ToWide(serverName);
    const SEC_WCHAR* targetName = targetNameWide.empty() ? nullptr : reinterpret_cast<const SEC_WCHAR*>(targetNameWide.c_str());

    SECURITY_STATUS status = InitializeSecurityContextW(
        &credHandle_,
        nullptr,
        const_cast<SEC_WCHAR*>(targetName),
        contextFlags,
        0,
        SECURITY_NATIVE_DREP,
        nullptr,
        0,
        &ctxtHandle_,
        &outBufferDesc,
        &outFlags,
        &expiry);

    if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_OK) {
        error = "InitializeSecurityContext(initial) failed: " + StatusToHex(status);
        return false;
    }

    contextReady_ = true;

    if (outBuffer.cbBuffer > 0 && outBuffer.pvBuffer != nullptr) {
        const int sent = send(socketHandle_, static_cast<const char*>(outBuffer.pvBuffer), static_cast<int>(outBuffer.cbBuffer), 0);
        FreeContextBuffer(outBuffer.pvBuffer);
        outBuffer.pvBuffer = nullptr;
        if (sent == SOCKET_ERROR) {
            error = "Failed to send client handshake token.";
            return false;
        }
    }

    encryptedDataLen_ = 0;
    while (status == SEC_I_CONTINUE_NEEDED || status == SEC_E_INCOMPLETE_MESSAGE) {
        if (status == SEC_E_INCOMPLETE_MESSAGE || encryptedDataLen_ == 0) {
            if (!ReadFromSocket()) {
                error = "Failed to receive server handshake token.";
                return false;
            }
        }

        SecBuffer inBuffers[2]{};
        inBuffers[0].BufferType = SECBUFFER_TOKEN;
        inBuffers[0].pvBuffer = encryptedBuffer_.data();
        inBuffers[0].cbBuffer = static_cast<unsigned long>(encryptedDataLen_);
        inBuffers[1].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc inBufferDesc{};
        inBufferDesc.cBuffers = 2;
        inBufferDesc.pBuffers = inBuffers;
        inBufferDesc.ulVersion = SECBUFFER_VERSION;

        outBuffer = {};
        outBuffer.BufferType = SECBUFFER_TOKEN;
        outBufferDesc = {};
        outBufferDesc.cBuffers = 1;
        outBufferDesc.pBuffers = &outBuffer;
        outBufferDesc.ulVersion = SECBUFFER_VERSION;

        status = InitializeSecurityContextW(
            &credHandle_,
            &ctxtHandle_,
            const_cast<SEC_WCHAR*>(targetName),
            contextFlags,
            0,
            SECURITY_NATIVE_DREP,
            &inBufferDesc,
            0,
            nullptr,
            &outBufferDesc,
            &outFlags,
            &expiry);

        if ((status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) && outBuffer.cbBuffer > 0 && outBuffer.pvBuffer != nullptr) {
            const int sent = send(socketHandle_, static_cast<const char*>(outBuffer.pvBuffer), static_cast<int>(outBuffer.cbBuffer), 0);
            FreeContextBuffer(outBuffer.pvBuffer);
            outBuffer.pvBuffer = nullptr;
            if (sent == SOCKET_ERROR) {
                error = "Failed to send client handshake continuation token.";
                return false;
            }
        }

        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            continue;
        }

        if (status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) {
            error = "InitializeSecurityContext(continue) failed: " + StatusToHex(status);
            return false;
        }

        if (inBuffers[1].BufferType == SECBUFFER_EXTRA) {
            const size_t extra = inBuffers[1].cbBuffer;
            memmove(encryptedBuffer_.data(),
                    encryptedBuffer_.data() + (encryptedDataLen_ - extra),
                    extra);
            encryptedDataLen_ = extra;
        } else {
            encryptedDataLen_ = 0;
        }
    }

    return true;
}

// [GROUP: TLS Setup]
bool TlsSocket::PerformServerHandshake(string& error) {
    DWORD contextFlags = ASC_REQ_SEQUENCE_DETECT |
                         ASC_REQ_REPLAY_DETECT |
                         ASC_REQ_CONFIDENTIALITY |
                         ASC_REQ_ALLOCATE_MEMORY |
                         ASC_REQ_STREAM;

    DWORD outFlags = 0;
    TimeStamp expiry{};
    SECURITY_STATUS status = SEC_I_CONTINUE_NEEDED;

    encryptedDataLen_ = 0;

    while (status == SEC_I_CONTINUE_NEEDED || status == SEC_E_INCOMPLETE_MESSAGE) {
        if (status == SEC_E_INCOMPLETE_MESSAGE || encryptedDataLen_ == 0) {
            if (!ReadFromSocket()) {
                error = "Failed to receive client handshake token.";
                return false;
            }
        }

        SecBuffer inBuffers[2]{};
        inBuffers[0].BufferType = SECBUFFER_TOKEN;
        inBuffers[0].pvBuffer = encryptedBuffer_.data();
        inBuffers[0].cbBuffer = static_cast<unsigned long>(encryptedDataLen_);
        inBuffers[1].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc inBufferDesc{};
        inBufferDesc.cBuffers = 2;
        inBufferDesc.pBuffers = inBuffers;
        inBufferDesc.ulVersion = SECBUFFER_VERSION;

        SecBuffer outBuffer{};
        outBuffer.BufferType = SECBUFFER_TOKEN;
        SecBufferDesc outBufferDesc{};
        outBufferDesc.cBuffers = 1;
        outBufferDesc.pBuffers = &outBuffer;
        outBufferDesc.ulVersion = SECBUFFER_VERSION;

        status = AcceptSecurityContext(
            &credHandle_,
            contextReady_ ? &ctxtHandle_ : nullptr,
            &inBufferDesc,
            contextFlags,
            SECURITY_NATIVE_DREP,
            &ctxtHandle_,
            &outBufferDesc,
            &outFlags,
            &expiry);

        if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) {
            contextReady_ = true;
        }

        if ((status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) && outBuffer.cbBuffer > 0 && outBuffer.pvBuffer != nullptr) {
            const int sent = send(socketHandle_, static_cast<const char*>(outBuffer.pvBuffer), static_cast<int>(outBuffer.cbBuffer), 0);
            FreeContextBuffer(outBuffer.pvBuffer);
            outBuffer.pvBuffer = nullptr;
            if (sent == SOCKET_ERROR) {
                error = "Failed to send server handshake token.";
                return false;
            }
        }

        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            continue;
        }

        if (status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) {
            error = "AcceptSecurityContext failed: " + StatusToHex(status);
            return false;
        }

        if (inBuffers[1].BufferType == SECBUFFER_EXTRA) {
            const size_t extra = inBuffers[1].cbBuffer;
            memmove(encryptedBuffer_.data(),
                    encryptedBuffer_.data() + (encryptedDataLen_ - extra),
                    extra);
            encryptedDataLen_ = extra;
        } else {
            encryptedDataLen_ = 0;
        }
    }

    return true;
}

// [GROUP: TLS Setup]
bool TlsSocket::QueryStreamSizes(string& error) {
    SecPkgContext_StreamSizes streamSizes{};
    const SECURITY_STATUS status = QueryContextAttributes(
        &ctxtHandle_,
        SECPKG_ATTR_STREAM_SIZES,
        &streamSizes);

    if (status != SEC_E_OK) {
        error = "QueryContextAttributes(SECPKG_ATTR_STREAM_SIZES) failed: " + StatusToHex(status);
        return false;
    }

    cbHeader_ = streamSizes.cbHeader;
    cbTrailer_ = streamSizes.cbTrailer;
    cbMaximumMessage_ = streamSizes.cbMaximumMessage;
    streamSizesReady_ = true;
    return true;
}

// [GROUP: TLS Setup]
bool TlsSocket::InitializeClient(SOCKET socketHandle,
                                 const string& serverName,
                                 bool insecureSkipVerify,
                                 string& error) {
    socketHandle_ = socketHandle;
    encryptedBuffer_.assign(kHandshakeRecvChunk, '\0');
    decryptedBuffer_.clear();
    decryptedOffset_ = 0;
    encryptedDataLen_ = 0;

    if (!AcquireClientCredentials(insecureSkipVerify, error)) {
        return false;
    }

    if (!PerformClientHandshake(serverName, error)) {
        return false;
    }

    if (!QueryStreamSizes(error)) {
        return false;
    }

    return true;
}

// [GROUP: TLS Setup]
bool TlsSocket::InitializeServer(SOCKET socketHandle,
                                 const string& pfxPath,
                                 const string& pfxPassword,
                                 string& error) {
    socketHandle_ = socketHandle;
    encryptedBuffer_.assign(kHandshakeRecvChunk, '\0');
    decryptedBuffer_.clear();
    decryptedOffset_ = 0;
    encryptedDataLen_ = 0;

    if (!AcquireServerCredentials(pfxPath, pfxPassword, error)) {
        return false;
    }

    if (!PerformServerHandshake(error)) {
        return false;
    }

    if (!QueryStreamSizes(error)) {
        return false;
    }

    return true;
}

// [GROUP: Framed Transport]
bool TlsSocket::SendAll(const char* data, int totalBytes) {
    if (!streamSizesReady_ || data == nullptr || totalBytes < 0) {
        return false;
    }

    int sentBytes = 0;
    while (sentBytes < totalBytes) {
        const int chunk = min(totalBytes - sentBytes, static_cast<int>(cbMaximumMessage_));
        vector<char> encryptedRecord(static_cast<size_t>(cbHeader_ + chunk + cbTrailer_));

        SecBuffer buffers[4]{};
        buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
        buffers[0].pvBuffer = encryptedRecord.data();
        buffers[0].cbBuffer = cbHeader_;

        buffers[1].BufferType = SECBUFFER_DATA;
        buffers[1].pvBuffer = encryptedRecord.data() + cbHeader_;
        buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
        memcpy(buffers[1].pvBuffer, data + sentBytes, static_cast<size_t>(chunk));

        buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buffers[2].pvBuffer = encryptedRecord.data() + cbHeader_ + chunk;
        buffers[2].cbBuffer = cbTrailer_;

        buffers[3].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc desc{};
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = buffers;

        const SECURITY_STATUS status = EncryptMessage(&ctxtHandle_, 0, &desc, 0);
        if (status != SEC_E_OK) {
            return false;
        }

        const int tlsPacketSize = static_cast<int>(buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer);
        int tlsPacketSent = 0;
        while (tlsPacketSent < tlsPacketSize) {
            const int sent = send(
                socketHandle_,
                encryptedRecord.data() + tlsPacketSent,
                tlsPacketSize - tlsPacketSent,
                0);
            if (sent == SOCKET_ERROR) {
                return false;
            }
            tlsPacketSent += sent;
        }

        sentBytes += chunk;
    }

    return true;
}

// [GROUP: Internal Transport Helpers]
bool TlsSocket::DecryptMoreData() {
    while (true) {
        if (encryptedDataLen_ == 0) {
            if (!ReadFromSocket()) {
                return false;
            }
        }

        SecBuffer buffers[4]{};
        buffers[0].BufferType = SECBUFFER_DATA;
        buffers[0].pvBuffer = encryptedBuffer_.data();
        buffers[0].cbBuffer = static_cast<unsigned long>(encryptedDataLen_);
        buffers[1].BufferType = SECBUFFER_EMPTY;
        buffers[2].BufferType = SECBUFFER_EMPTY;
        buffers[3].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc desc{};
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = buffers;

        SECURITY_STATUS status = DecryptMessage(&ctxtHandle_, &desc, 0, nullptr);
        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            if (!ReadFromSocket()) {
                return false;
            }
            continue;
        }

        if (status == SEC_I_CONTEXT_EXPIRED) {
            return false;
        }

        if (status != SEC_E_OK) {
            return false;
        }

        char* plainPtr = nullptr;
        unsigned long plainLen = 0;
        size_t extraLen = 0;

        for (int i = 1; i < 4; ++i) {
            if (buffers[i].BufferType == SECBUFFER_DATA) {
                plainPtr = static_cast<char*>(buffers[i].pvBuffer);
                plainLen = buffers[i].cbBuffer;
            } else if (buffers[i].BufferType == SECBUFFER_EXTRA) {
                extraLen = buffers[i].cbBuffer;
            }
        }

        if (extraLen > 0) {
            memmove(encryptedBuffer_.data(),
                    encryptedBuffer_.data() + (encryptedDataLen_ - extraLen),
                    extraLen);
            encryptedDataLen_ = extraLen;
        } else {
            encryptedDataLen_ = 0;
        }

        if (plainPtr != nullptr && plainLen > 0) {
            decryptedBuffer_.assign(plainPtr, plainPtr + plainLen);
            decryptedOffset_ = 0;
            return true;
        }
    }
}

// [GROUP: Framed Transport]
bool TlsSocket::RecvAll(char* data, int totalBytes) {
    if (!streamSizesReady_ || data == nullptr || totalBytes < 0) {
        return false;
    }

    int copied = 0;
    while (copied < totalBytes) {
        if (decryptedOffset_ < decryptedBuffer_.size()) {
            const size_t available = decryptedBuffer_.size() - decryptedOffset_;
            const int take = min(static_cast<int>(available), totalBytes - copied);
            memcpy(data + copied, decryptedBuffer_.data() + decryptedOffset_, static_cast<size_t>(take));
            copied += take;
            decryptedOffset_ += static_cast<size_t>(take);
            continue;
        }

        decryptedBuffer_.clear();
        decryptedOffset_ = 0;

        if (!DecryptMoreData()) {
            return false;
        }
    }

    return true;
}

// [GROUP: Lifecycle]
void TlsSocket::Shutdown() {
    if (contextReady_) {
        DeleteSecurityContext(&ctxtHandle_);
        memset(&ctxtHandle_, 0, sizeof(ctxtHandle_));
        contextReady_ = false;
    }

    if (credentialsReady_) {
        FreeCredentialsHandle(&credHandle_);
        memset(&credHandle_, 0, sizeof(credHandle_));
        credentialsReady_ = false;
    }

    if (serverCertificateContext_ != nullptr) {
        CertFreeCertificateContext(serverCertificateContext_);
        serverCertificateContext_ = nullptr;
    }

    streamSizesReady_ = false;
    cbHeader_ = 0;
    cbTrailer_ = 0;
    cbMaximumMessage_ = 0;

    encryptedBuffer_.clear();
    decryptedBuffer_.clear();
    decryptedOffset_ = 0;
    encryptedDataLen_ = 0;
}
