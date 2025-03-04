set_xmakever("2.9.8")
set_project("OmniMIDIv2")

set_allowedplats("windows", "linux")
set_allowedmodes("debug", "release")
set_allowedarchs("x86", "x86_64", "arm64")
	
add_rules("mode.release", "mode.debug")
set_languages("clatest", "cxxlatest")

target("OmniMIDI")
	set_kind("shared")

	if is_mode("debug") then
		add_defines("DEBUG")
		set_symbols("debug")
		set_optimize("none")
	else	
		add_defines("NDEBUG")
		set_symbols("none")
		set_optimize("fastest")
	end
	
	add_defines("OMNIMIDI_EXPORTS")

	add_ldflags("-j")

	add_includedirs("inc")
	add_files("*.cpp")

	if is_plat("windows") then	
		set_toolchains("mingw")

		-- Remove lib prefix
		set_prefixname("")

		add_cxxflags("-fexperimental-library")
		add_syslinks("winmm", "uuid", "shlwapi", "shell32", "user32", "gdi32", "ole32", "-l:libunwind.a")
		add_defines("_WIN32", "_WIN32_WINNT=0x6000")

		add_files("OMRes.rc")
		remove_files("UnixEntry.cpp")
	else
		set_toolchains("gcc")

		add_cxxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden")
		add_ldflags("-shared")

		remove_files("bassasio.cpp")
		remove_files("basswasapi.cpp")
		remove_files("WDMEntry.cpp")
		remove_files("StreamPlayer.cpp")
		remove_files("WDMDrv.cpp")
		remove_files("WDMEntry.cpp")	
	end
target_end()