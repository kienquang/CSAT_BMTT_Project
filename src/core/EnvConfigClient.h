#pragma once

// Compatibility shim: redirect legacy EnvConfigClient usage to EnvConfig
#include "EnvConfig.h"
using EnvConfigClient = EnvConfig;
