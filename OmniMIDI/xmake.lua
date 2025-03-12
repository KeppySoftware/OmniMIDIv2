set_xmakever("2.9.8")
set_project("OmniMIDIv2")

set_allowedplats("windows", "linux")
set_allowedmodes("debug", "release")
set_allowedarchs("x86", "x64", "x86_64", "arm64")
	
add_rules("mode.release", "mode.debug")
set_languages("clatest", "cxx2a", "c++20")
set_runtimes("stdc++_static")

target("OmniMIDI")
	if is_plat("linux") then 	
		set_kind("binary")

		if is_mode("debug") then
			add_defines("DEBUG")
			add_defines("_DEBUG")
			set_symbols("debug")
			set_optimize("none")
		else	
			add_defines("NDEBUG")
			set_symbols("hidden")
			set_optimize("fastest")
			set_strip("all")
		end

		add_defines("OM_STANDALONE")

		add_includedirs("inc")
		add_files("src/*.cpp")
		add_files("src/weak_libjack.c")

		set_toolchains("gcc")

		add_cxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden")
		add_syslinks("asound")

		add_shflags("-pie", "-Wl,-E", { force = true })

		remove_files("bassasio.cpp")
		remove_files("basswasapi.cpp")
		remove_files("WDMEntry.cpp")
		remove_files("StreamPlayer.cpp")
		remove_files("WDMDrv.cpp")
		remove_files("WDMEntry.cpp")
	end
target_end()

target("libOmniMIDI")
	set_kind("shared")
	set_basename("OmniMIDI")

	if is_mode("debug") then
		add_defines("DEBUG")
		add_defines("_DEBUG")
		set_symbols("debug")
		set_optimize("none")
	else	
		add_defines("NDEBUG")
		set_symbols("hidden")
		set_optimize("fastest")
		set_strip("all")
	end
	
	add_defines("OMNIMIDI_EXPORTS")
	add_ldflags("-j")

	add_includedirs("inc")
	add_files("src/*.cpp")

	if is_plat("windows") then	
		set_toolchains("mingw")

		-- Remove lib prefix
		set_prefixname("")
		add_cxxflags("clang::-fexperimental-library", { force = true })
		add_shflags("-static-libgcc", { force = true })
		add_syslinks("winmm", "uuid", "shlwapi", "ole32", "-l:libwinpthread.a")
		add_defines("_WIN32", "_WINXP", "_WIN32_WINNT=0x6000")

		remove_files("UnixEntry.cpp")
	else
		set_toolchains("gcc")

		add_cxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden")
		add_syslinks("asound")

		remove_files("bassasio.cpp")
		remove_files("basswasapi.cpp")
		remove_files("WDMEntry.cpp")
		remove_files("StreamPlayer.cpp")
		remove_files("WDMDrv.cpp")
		remove_files("WDMEntry.cpp")	
	end
target_end()