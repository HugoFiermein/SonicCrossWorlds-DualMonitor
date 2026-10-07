#pragma once

#include <string>

#define FIX_NAME "CrossWorldsFix"
#define REPO_URL "https://codeberg.org/Lyall/CrossWorldsFix"

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 3

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

inline const std::string sFixVersion = VERSION_STRING;
inline const std::string sFixName = FIX_NAME;

#define COMPANY_NAME      "Lyall"
#define PRODUCT_NAME      FIX_NAME
#define PRODUCT_VERSION   VERSION_STRING
#define FILE_VERSION      VERSION_STRING
#define LEGAL_COPYRIGHT   "© 2025 Lyall. Licensed under the MIT License."
#define LEGAL_TRADEMARKS  ""
#define COMMENTS          ""
#define FILE_DESCRIPTION_ASI     FIX_NAME " ASI Plugin"
#define INTERNAL_NAME_ASI        FIX_NAME ".asi"
#define ORIGINAL_FILENAME_ASI    FIX_NAME ".asi"
