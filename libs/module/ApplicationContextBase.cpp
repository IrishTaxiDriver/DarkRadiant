#include "ApplicationContextBase.h"

#include <locale>
#include "string/string.h"
#include "debugging/debugging.h"
#include "itextstream.h"
#include "iregistry.h"
#include "os/fs.h"
#include "os/path.h"
#include "os/dir.h"
#include "string/encoding.h"

#ifdef WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace radiant
{

namespace
{
	const std::string PLUGINS_DIR = "plugins/"; ///< name of plugins directory
	const std::string MODULES_DIR = "modules/"; ///< name of modules directory
}

/**
 * Return the application path of the current Radiant instance.
 */
std::string ApplicationContextBase::getApplicationPath() const
{
	return _appPath;
}

std::vector<std::string> ApplicationContextBase::getLibraryPaths() const
{
	auto libBasePath = os::standardPathWithSlash(getLibraryBasePath());
	
#if defined(__APPLE__) && defined(DR_MODULES_NEXT_TO_APP)
	// Xcode output goes to the application folder right now
	return { libBasePath };
#else
	return 
	{ 
		libBasePath + MODULES_DIR, 
		libBasePath + PLUGINS_DIR 
	};
#endif
}

std::string ApplicationContextBase::getLibraryBasePath() const
{
#if defined(__APPLE__)
	return _appPath;
#elif defined(POSIX)
#   if defined(PKGLIBDIR) && !defined(ENABLE_RELOCATION)
	return PKGLIBDIR;
#   else
	return _appPath + "../lib/darkradiant/";
#   endif
#else // !defined(POSIX)
	return _appPath;
#endif
}

std::string ApplicationContextBase::getRuntimeDataPath() const
{
#if defined(__APPLE__)
    // The Resources are in the Bundle folder Contents/Resources/, whereas the
    // application binary is located in Contents/MacOS/
    std::string path = getApplicationPath() + "../Resources/";
    
    // When launching the app from Xcode, the Resources/ folder
    // is next to the binary
    if (!fs::exists(path))
    {
        path = getApplicationPath() + "Resources/";
    }
    
    return path;
#elif defined(POSIX)
#   if defined(PKGDATADIR) && !defined(ENABLE_RELOCATION)
    return std::string(PKGDATADIR) + "/";
#   else
    return _appPath + "../share/darkradiant/";
#   endif
#else
    return getApplicationPath();
#endif
}

std::string ApplicationContextBase::getHTMLPath() const
{
#if defined(POSIX)
#if defined(HTMLDIR) && !defined(ENABLE_RELOCATION)
    return std::string(HTMLDIR) + "/";
#else
    return _appPath + "../share/doc/darkradiant/";
#endif
#else
    // TODO: implement correct path for macOS and Windows
    return getRuntimeDataPath();
#endif
}

std::string ApplicationContextBase::getSettingsPath() const
{
	return _settingsPath;
}

std::string ApplicationContextBase::getBitmapsPath() const
{
	return getRuntimeDataPath() + "bitmaps/";
}

const IApplicationContext::ArgumentList& ApplicationContextBase::getCmdLineArgs() const
{
	return _cmdLineArgs;
}

// ============== OS-Specific Implementations go here ===================

// ================ POSIX ====================
#if defined(POSIX) || defined(__APPLE__)

#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#endif

namespace
{

#ifdef __APPLE__

// greebo: In OSX, we use the proc_pidpath() function
// to determine the absolute path of this PID
std::string getExecutablePath(char* argv[])
{
    pid_t pid = getpid();

    char pathBuf[PROC_PIDPATHINFO_MAXSIZE];
    int ret = proc_pidpath(pid, pathBuf, sizeof(pathBuf));

    if (ret > 0)
    {
        // Success
        fs::path execPath = std::string(pathBuf);
        fs::path appPath = execPath.remove_leaf();

        rConsole() << "Application path: " << appPath << std::endl;

        return appPath.string();
    }

    // Error, terminate the app
    rConsoleError() << "ApplicationContextBase: could not get app path: "
        << strerror(errno) << std::endl;

    throw std::runtime_error("ApplicationContextBase: could not get app path");
}

#else // generic POSIX

const char* LINK_NAME =
#if defined (__linux__)
  "/proc/self/exe"
#else // FreeBSD
  "/proc/curproc/file"
#endif
;

/// brief Returns the filename of the executable belonging to the current process, or 0 if not found.
std::string getExecutablePath(char* argv[])
{
    char buf[PATH_MAX];

	// Now read the symbolic link
	int ret = readlink(LINK_NAME, buf, PATH_MAX);

	if (ret == -1)
    {
		rMessage() << "getexename: falling back to argv[0]: '" << argv[0] << "'\n";

		const char* path = realpath(argv[0], buf);

		if (path == nullptr)
        {
			// In case of an error, leave the handling up to the caller
			return std::string();
		}
	}

	/* Ensure proper NUL termination */
	buf[ret] = '\0';

	/* delete the program name */
	*(strrchr(buf, '/')) = '\0';

	// NOTE: we build app path with a trailing '/'
	// it's a general convention in Radiant to have the slash at the end of directories
	if (buf[strlen(buf)-1] != '/')
    {
		strcat(buf, "/");
	}

	return std::string(buf);
}

#endif

}

void ApplicationContextBase::initialise(int argc, char* argv[])
{
	// Give away unnecessary root privileges.
	// Important: must be done before calling gtk_init().
	char *loginname;
	struct passwd *pw;
	seteuid(getuid());

	if (geteuid() == 0 &&
		(loginname = getlogin()) != 0 &&
		(pw = getpwnam(loginname)) != 0)
	{
		setuid(pw->pw_uid);
	}

	initArgs(argc, argv);

    // Initialise the home directory path
    std::string homedir = getenv("HOME");
    std::string home = os::standardPathWithSlash(homedir) + ".darkradiant/";
    os::makeDirectory(home);
    _homePath = home;

	_appPath = getExecutablePath(argv);
    ASSERT_MESSAGE(!_appPath.empty(), "failed to deduce app path");

	// Initialise the relative paths
	initPaths();
}

// ================ WIN32 ====================
#elif defined(WIN32)

void ApplicationContextBase::initialise(int argc, char* argv[])
{
	initArgs(argc, argv);

    // Get application data directory from environment
	std::string appData = getenv("APPDATA");
	if (appData.empty())
    {
		throw std::runtime_error(
            "Critical: cannot find APPDATA environment variable."
        );
	}

    // Construct DarkRadiant home directory
	_homePath = appData + "\\DarkRadiant";
	if (!os::makeDirectory(_homePath))
    {
        rConsoleError() << "ApplicationContextBase: could not create home directory "
                  << "'" << _homePath << "'" << std::endl;
    }

	{
		// get path to the editor
		wchar_t filename[MAX_PATH+1];
		GetModuleFileName(0, filename, MAX_PATH);
		wchar_t* last_separator = wcsrchr(filename, '\\');
		if (last_separator != 0) {
			*(last_separator+1) = '\0';
		}
		else {
			filename[0] = '\0';
		}

		// convert to std::string
		std::wstring wide(filename);
		std::string appPathNarrow = string::unicode_to_mb(wide);

		// Make sure we have forward slashes
		_appPath = os::standardPath(appPathNarrow);
	}
	// Initialise the relative paths
	initPaths();
}

#else
#error "unsupported platform"
#endif

// ============== OS-Specific Implementations end ===================

void ApplicationContextBase::initArgs(int argc, char* argv[])
{
	// Store the arguments locally, ignore the first one
	for (int i = 1; i < argc; i++) {
		_cmdLineArgs.push_back(argv[i]);
	}
}

void ApplicationContextBase::initPaths()
{
	// Ensure that the homepath ends with a slash
	_homePath = os::standardPathWithSlash(_homePath);
	_appPath = os::standardPathWithSlash(_appPath);

	// Make sure the home/settings folder exists (attempt to create it)
	_settingsPath = _homePath;
	if (!os::makeDirectory(_settingsPath))
    {
        rConsoleError() << "ApplicationContextBase: unable to create settings path '"
                  << _settingsPath << "'" << std::endl;
    }
}

const ErrorHandlingFunction& ApplicationContextBase::getErrorHandlingFunction() const
{
	return _errorHandler;
}

void ApplicationContextBase::setErrorHandlingFunction(const ErrorHandlingFunction& function)
{
	_errorHandler = function;
}

} // namespace module
