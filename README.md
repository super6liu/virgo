virgo-like-win10
=====
Virtual Desktop Manager for Windows  
Forked from [henkman/virgo](https://github.com/henkman/virgo)

[Download](https://github.com/super6liu/virgo/releases/download/2.0.1/virgo-like-win10.zip)

Features:  
- provide a Win10-style hotkey virtual workspace manager (tested on Win7 SP1)
- up to 4 virtual desktops

Hotkeys:

        CTRL + WIN + LEFT      -> switch to previous desktop  
        CTRL + WIN + RIGHT     -> switch to next desktop  
        CTRL + WIN + D         -> create a new desktop (up to 4)  
        CTRL + WIN + F4        -> delete current desktop and move all open windows to privous desktop

To exit:
- kill virgo-like-win10.exe in Task Manager, or log off

==================================================

The nerds can build it with

        git clone https://github.com/super6liu/virgo.git
        cd virgo
        make

If you do not have gcc/make installed you can change that doing following

1. go to http://msys2.github.io/ and install the 32-bit (i686) version
2. open msys2 shell and install mingw-w64-i686-gcc and mingw-w64-i686-make using pacman
3. duplicate C:\msys32\mingw32\bin\mingw32-make.exe and name it make.exe
