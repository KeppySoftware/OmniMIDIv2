set_xmakever("2.9.5")
set_project("OmniMIDIv2")

set_allowedplats("windows", "linux")
set_allowedmodes("debug", "release")
set_allowedarchs("x86", "x64", "x86_64", "arm64")
	
add_rules("mode.release", "mode.debug")
set_languages("clatest", "cxx2a", "c++20")
set_runtimes("stdc++_static")

option("nonfree")
    set_default(false)
    set_showmenu(true)
	-- Does not work???
	-- add_defines("_NONFREE")

option("statsdev")
	set_default(false)
	set_showmenu(true)
	-- Does not work???
	-- add_defines("_STATSDEV")

option("useclang")
    set_default(is_plat("linux"))
    set_showmenu(true)

-- Self-hosted MIDI out for Linux
target("OmniMIDI")		
	if is_plat("windows") then 	
		-- Dummy
		set_enabled(false)	
	else 	
		set_options("nonfree")
		set_options("statsdev")
		set_options("useclang")

		set_kind("binary")

		if has_config("useclang") then
			set_toolchains("clang")
		else
			set_toolchains("gcc")
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

		add_defines("OM_STANDALONE")

		add_includedirs("inc")
		add_files("src/*.cpp")

		add_cxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden", "-Wdangling-else")
		add_syslinks("asound")

		add_shflags("-pie", "-Wl,-E", { force = true })

		if not has_config("nonfree") then
			remove_files("src/bass*.c*")
		end

		-- ASIO and WASAPI not available under Linux/FreeBSD
		remove_files("src/bassasio.cpp")
		remove_files("src/basswasapi.cpp")

		-- Windows stuff
		remove_files("src/WDM*.cpp")
		remove_files("src/StreamPlayer.cpp")
	end
target_end()

-- Actual lib (or user-mode driver, under Win32)
target("libOmniMIDI")
	set_kind("shared")
	set_basename("OmniMIDI")

	set_options("nonfree")
	set_options("statsdev")
	set_options("useclang")

	if has_config("useclang") then
		if is_plat("windows") then
			set_toolchains("clang-cl")
		else 
			set_toolchains("clang")
		end
	else
		if is_plat("windows") then
			set_toolchains("mingw")
		else 
			set_toolchains("gcc")
		end
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
	
	add_defines("OMNIMIDI_EXPORTS")
	add_ldflags("-j")
	add_cxflags("-Wall", "-Wdangling-else")

	add_includedirs("inc")
	add_files("src/*.cpp")

	if not has_config("nonfree") then
		remove_files("src/bass*.c*")
	end

	if is_plat("windows") then
		-- Remove lib prefix
		set_prefixname("")
		add_cxxflags("clang::-fexperimental-library", { force = true })
		add_shflags("-static-libgcc", { force = true })
		add_syslinks("uuid", "shlwapi", "ole32", "user32", "shell32")
		add_defines("_WIN32", "_WIN32_WINNT=0x6000")

		if is_mode("debug") then 
			add_syslinks("-l:libwinpthread.a")
		end

		remove_files("src/UnixEntry.cpp")
	else
		add_cxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden")

		-- ASIO and WASAPI not available under Linux/FreeBSD
		remove_files("src/bassasio.cpp")
		remove_files("src/basswasapi.cpp")

		-- Windows stuff
		remove_files("src/WDM*.cpp")
		remove_files("src/StreamPlayer.cpp")
	end
target_end()