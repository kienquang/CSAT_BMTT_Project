#ifndef TLS_SOCKET_H
#define TLS_SOCKET_H

#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif

#include <winsock2.h>
#include <security.h>
#include <wincrypt.h>

#include <string>
#include <vector>

class TlsSocket {
public:
    // [GROUP: Lifecycle]
    TlsSocket();
    ~TlsSocket();

    // [GROUP: TLS Setup]
    bool InitializeClient(SOCKET socketHandle,
                          const std::string& serverName,
                          bool insecureSkipVerify,
                          std::string& error);
    bool InitializeServer(SOCKET socketHandle,
                          const std::string& pfxPath,
                          const std::string& pfxPassword,
                          std::string& error);

    // [GROUP: Framed Transport]
    bool SendAll(const char* data, int totalBytes);
    bool RecvAll(char* data, int totalBytes);

    // [GROUP: Lifecycle]
    void Shutdown();

private:
    // [GROUP: TLS Setup]
    bool AcquireClientCredentials(bool insecureSkipVerify, std::string& error);
    bool AcquireServerCredentials(const std::string& pfxPath,
                                  const std::string& pfxPassword,
                                  std::string& error);
    bool PerformClientHandshake(const std::string& serverName, std::string& error);
    bool PerformServerHandshake(std::string& error);
    bool QueryStreamSizes(std::string& error);

    // [GROUP: Internal Transport Helpers]
    bool ReadFromSocket();
    bool DecryptMoreData();

    SOCKET socketHandle_;
    bool credentialsReady_;
    bool contextReady_;
    bool streamSizesReady_;
    CredHandle credHandle_;
    CtxtHandle ctxtHandle_;

    unsigned long cbHeader_;
    unsigned long cbTrailer_;
    unsigned long cbMaximumMessage_;

    PCCERT_CONTEXT serverCertificateContext_;

    std::vector<char> encryptedBuffer_;
    std::vector<char> decryptedBuffer_;
    size_t decryptedOffset_;
    size_t encryptedDataLen_;
};

#endif  // TLS_SOCKET_H
