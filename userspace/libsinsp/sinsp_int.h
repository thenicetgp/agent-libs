////////////////////////////////////////////////////////////////////////////
// Public definitions for the scap library
////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif
#include <assert.h>

#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <exception>
#include <sstream>
#include <set>
#include <deque>
#include <queue>
#include <list>
#include <vector>
#include <iostream>

using namespace std;

#include "../libscap/scap.h"
#include "parser_http.h"
#include "settings.h"
#include "utils.h"
#include "event.h"
#include "../libscap/scap.h"
#include "transactinfo.h"
#include "procinfo.h"
#include "fdinfo.h"
#include "parsers.h"
#include "ifinfo.h"
#include "internal_metrics.h"

#ifndef MIN
#define MIN(X,Y) ((X) < (Y)? (X):(Y))
#define MAX(X,Y) ((X) > (Y)? (X):(Y))
#endif

//
// ASSERT implementation
//
#ifdef _DEBUG
#ifdef ASSERT_TO_LOG
#define ASSERT(X) g_logger.log(sinsp_logger::SEV_ERROR, "ASSERTION %s at %s:%d", #X , __FILE__, __LINE__)
#else
#define ASSERT(X) assert(X)
#endif // ASSERT_TO_LOG
#else // _DEBUG
#define ASSERT(X)
#endif // _DEBUG

//
// Public export macro
//
#ifdef _WIN32
#define SINSP_PUBLIC __declspec(dllexport)
#define BRK(X) {if(evt->get_num() == X)__debugbreak();}
#else
#define SINSP_PUBLIC
#endif

//
// Path separator
//
#ifdef _WIN32
#define DIR_PATH_SEPARATOR '\\'
#else
#define DIR_PATH_SEPARATOR '/'
#endif

//
// The logger
//
extern sinsp_logger g_logger;
