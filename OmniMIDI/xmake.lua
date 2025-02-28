add_rules("mode.release")
set_defaultarchs("linux|x86", "linux|x64", "linux|arm64", "windows|x86", "windows|x64", "windows|arm64")

target("OmniMIDI")
	set_kind("shared")

	set_toolchains("mingw")
	set_optimize("fastest")

	-- Statically link libunwind
	add_defines("NDEBUG", "OMNIMIDI_EXPORTS")

	set_languages("clatest", "cxxlatest")
	add_cxxflags("-fexperimental-library")
	add_ldflags("-j")

	add_includedirs("inc")
	add_files("*.cpp")
	add_files("*.c")

	if is_plat("windows", "mingw") then
		-- Remove lib prefix
		set_prefixname("")
		add_syslinks("-l:libunwind.a")
		add_syslinks("winmm", "uuid", "shlwapi", "shell32", "user32", "gdi32", "ole32")
		add_defines("WIN7VERBOSE", "UNICODE", "_UNICODE", "_WIN32", "_WIN32_WINNT=0x5000")
		add_ldflags("-MUNICODE")
		add_files("OMRes.rc")
		remove_files("UnixEntry.cpp")
	else
		remove_files("StreamPlayer.cpp")
		remove_files("WDMDrv.cpp")
		remove_files("WDMEntry.cpp")
	end

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

