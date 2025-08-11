set_xmakever("2.9.5")
set_project("OmniMIDIv2")

set_allowedplats("mingw", "linux")
set_allowedmodes("debug", "release")
set_allowedarchs("x86", "x64", "x86_64", "arm64")
	
add_rules("mode.release", "mode.debug")
set_languages("clatest", "cxx2a", "c++20")
set_runtimes("stdc++_static")

add_requires("nlohmann_json", "miniaudio")

option("nonfree")
    set_default(false)
    set_showmenu(true)

option("statsdev")
	set_default(false)
	set_showmenu(true)

-- Self-hosted MIDI out for Linux
target("OmniMIDI")		
	if is_plat("mingw") then 	
		-- N/A for Windows
		set_enabled(false)	
	else
		set_kind("binary")
		add_defines("OM_STANDALONE")
		add_packages("nlohmann_json", "miniaudio")

		-- Option definitions
		set_options("nonfree")
		set_options("statsdev")

		if has_config("nonfree") then
			add_defines("_NONFREE")
		end

		if has_config("statsdev") then
			add_defines("_STATSDEV")
		end

		-- Target setup
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

		-- Sources
		add_includedirs("inc")
		add_files("src/**.cpp")

		-- Compiler setup
		add_cxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden", "-Wall", "-msse2")
		add_syslinks("asound")
		add_shflags("-pie", "-Wl,-E", { force = true })

		if not has_config("nonfree") then
			remove_files("src/bass*.c*")
		end

		-- ASIO and WASAPI not available under Linux/FreeBSD
		remove_files("src/synth/bassmidi/bassasio.cpp")
		remove_files("src/synth/bassmidi/basswasapi.cpp")

		-- Windows stuff
		remove_files("src/system/WDM*.cpp")
		remove_files("src/system/StreamPlayer.cpp")
	end
target_end()

-- Actual lib (or user-mode driver, under Win32)
target("libOmniMIDI")
	set_kind("shared")
	set_basename("OmniMIDI")
	add_defines("OMNIMIDI_EXPORTS")
	add_packages("nlohmann_json", "miniaudio")

	-- Option definitions
	set_options("nonfree")
	set_options("statsdev")

	if is_plat("mingw") then
		set_toolchains("mingw")
	end

	if has_config("nonfree") then
		add_defines("_NONFREE")
	end

	if has_config("statsdev") then
		add_defines("_STATSDEV")
	end

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

	-- Sources
	add_includedirs("inc")
	add_linkdirs("lib")
	add_files("src/**.cpp")

	-- Compiler setup
	add_ldflags("-j")
	add_cxflags("-Wall", "-msse2")

	if not has_config("nonfree") then
		remove_files("src/bass*.c*")
	end

	if is_plat("mingw") then
		-- Remove lib prefix
		set_prefixname("")
		add_cxxflags("clang::-fexperimental-library", { force = true })
		add_shflags("-static-libgcc", { force = true })

		add_syslinks("shlwapi", "ole32", "user32", "shell32", "uuid")
		add_defines("_WIN32", "_WIN32_WINNT=0x6000")

		if is_mode("debug") then 
			add_syslinks("-l:libwinpthread.a")
		end

		remove_files("src/system/UnixEntry.cpp")
	else
		add_cxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden")

		-- ASIO and WASAPI not available under Linux/FreeBSD
		remove_files("src/synth/bassmidi/bassasio.cpp")
		remove_files("src/synth/bassmidi/basswasapi.cpp")

		-- Windows stuff
		remove_files("src/system/WDM*.cpp")
		remove_files("src/system/StreamPlayer.cpp")
	end
target_end()