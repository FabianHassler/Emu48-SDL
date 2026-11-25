# Emu48/SDL Manual

**Emu48/SDL**, an HP38G/39G/40G/48SX/48GX/49G emulator for macOS and Linux.

Emu48/SDL is an unofficial port of Emu48 1.68 by Christoph Gießelink and Sébastien
Carlier. This manual is based on the manual and the debugger description of Emu48 for
Windows by Christoph Gießelink, adapted to macOS and Linux. The original Emu48 is on
<https://hp.giesselink.com/emu48.htm>.

## Contents

- [General](#general)
- [Acknowledgements](#acknowledgements)
- [Installation](#installation)
- [ROM images](#rom-images)
- [How to start](#how-to-start)
- [Command line](#command-line)
- [Virtual keyboard](#virtual-keyboard)
- [File menu](#file-menu)
- [Edit menu](#edit-menu)
- [View menu](#view-menu)
- [Tools menu](#tools-menu)
- [Help menu](#help-menu)
- [Debugger](#debugger-1)
- [MkShared](#mkshared)
- [Differences to Emu48 for Windows](#differences-to-emu48-for-windows)
- [License](#license)

## General

Emu48 is an emulator for the Hewlett Packard HP38G, HP39G, HP40G, HP48SX, HP48GX and HP49G
calculator hardware. These calculators are based on the 1LT8 Clarke (HP48SX) and on the
Yorke chip.

Emu48/SDL runs the unchanged emulation, debugger and dialog code of Emu48 1.68. Only the
window system is replaced by the SDL3 and Nuklear libraries. KML scripts, calculator images,
ROM images and state files are the same as for Emu48 for Windows.

## Acknowledgements

From Christoph Gießelink's manual: first of all a big thank to Sébastien Carlier for
publishing Emu48 v1.0 under the GPL. Without this decision newer versions of the emulator
wouldn't have been possible or ports to other similar calculators wouldn't have been made.
Also a big thank to Jean-Yves Avenard for his technical assistance in the beginning. Lode
Vandevenne spent the PNG image decoder, and thanks to all the unnamed authors for publishing
material about these calculators.

Emu48/SDL thanks Christoph Gießelink for Emu48 and the authors of SDL3 and Nuklear.

## Installation

**macOS** (macOS 11 or later): there are packages for Apple Silicon (`…-macOS-AppleSilicon.zip`)
and for Intel processors (`…-macOS-Intel.zip`). Unpack the zip file and copy the
`Emu48` folder to any place, e.g. into your Applications folder. The app isn't signed by
Apple, so macOS refuses to open it at the first start. Open it with a right click on
`Emu48.app` and *Open*, or allow it in *System Settings → Privacy & Security → Open Anyway*.
The same is done in a terminal with

    xattr -dr com.apple.quarantine Emu48.app

**Linux** (x86-64): unpack the tar.gz file and start `./emu48` in its folder. The program
needs the X11 and sound libraries of a desktop installation (PulseAudio or PipeWire). The
file dialogs need the xdg-desktop-portal service of the desktop or the `zenity` program.

Then copy your ROM images, KML scripts and calculator images into the **Emu48 directory**
and adjust the ROM image name to the name used in the corresponding KML script. At the
first start the Emu48 directory is the current directory: the folder with `Emu48.app` when
macOS starts the app (from the Finder or with `open`), the current directory of the terminal
when you start `emu48` on Linux or `Emu48.app/Contents/MacOS/Emu48` on macOS. It is saved in
the settings; you can change it in the dialog *Choose Your KML Script*.

The settings, which Emu48 for Windows keeps in the registry at `HKCU\Software\Emu48`, are
saved in the text file `Emu48.ini`:

- macOS: `~/Library/Application Support/Emu48/Emu48.ini`
- Linux: `~/.local/share/Emu48/Emu48.ini`

## ROM images

Emu48 needs a ROM image for each calculator model you want to emulate. ROM images are valid
in a packed (even address lower nibble, odd address higher nibble) or unpacked (one nibble
per byte with even address first) form.

Since fall 2000 the emulator ROMs for the HP38, 39, 40, 48 and 49 are freely available on
different Internet sites. Because there's no license for the distribution of the ROM images,
they aren't included in Emu48. Since Emu48 accepts packed ROM images, converting the given
ROM format (which is regularly a packed ROM image) is not necessary any more in most cases.
You can still use the classic way and extract them from your own calculator.

The HP48 ROM images are on [hpcalc.org](https://www.hpcalc.org/hp48/pc/emulators/), e.g. the
HP 48GX [revision R](https://www.hpcalc.org/details/4368) and the HP 48SX
[revision J](https://www.hpcalc.org/details/4371). They are packed ROM images and work
directly; rename them to the name used in the KML script, usually `ROM.48G` (from `gxrom-r`)
and `ROM.48S` (from `sxrom-j`).

The tools for creating ROM images are Windows programs from
[Christoph's page](https://hp.giesselink.com/emu48.htm). Run them on a Windows computer or
with Wine; the ROM images they create work in Emu48/SDL.

- **Convert** (in the Emu48 package for Windows) converts HP48 ROM images from the various
  formats of the 90s and clears the I/O register area that many upload programs filled with
  wrong content, so the selftest doesn't report an IROM fail:
  `Convert <old-rom-dump>` checks a ROM image and reports model, version and CRC result,
  `Convert <old-rom-dump> <new-rom-dump>` writes an unpacked and
  `Convert -p <old-rom-dump> <new-rom-dump>` a packed ROM image.
- **HP38G:** upload the ROM with the aplet "ROM UPLOAD", then `Convert -p <rom-file> ROM.38G`.
- **HP39G/HP40G:** upload the ROM with the aplet "ROM UPLOAD" and rename it to `ROM.39G`, or
  convert it to the unpacked format with the Rom2emu utility. There's also an HP39G/HP40G
  beta ROM for emulators inside an old Emu48 package.
- **HP48:** convert the ROM of another HP48 emulator with `Convert <rom-file> ROM.48G` (or
  `ROM.48S`). Without a ROM dump, use Jean-Yves Avenard's
  [ROMUPL.BIN](https://www.hpcalc.org/details/3686) or the ROMDump Wizard from Christoph's
  page, which get the ROM from your HP48 almost automatically.
- **HP49G:** there's no ROM download program, but the UPD49ROM tool creates a ROM image from
  a ROM update file, e.g. version 1.19-6: `UPD49ROM -f hp49119-6.flash ROM.49G`. This creates
  an HP49G ROM image with an empty user port 2.

## How to start

When you start Emu48 without a state file, you see the box *Choose Your KML Script*.

KML (Keyboard Mapping Language) scripts define the visual aspect of Emu48, the behaviour of
the buttons, of the keyboard, … It's a great way to customize your copy of Emu48.

Check in this dialog that the *Emu48 Directory* points to the directory with your KML
files. Click the refresh button (**V**) after modifying the directory path manually to
update the list box, or use the **…** button to choose the directory.

Choose a KML script for a calculator ROM you put into the Emu48 directory. KML scripts are
available on [hpcalc.org](https://www.hpcalc.org/hp48/pc/emulators/), e.g. the
[HP 48G(X) and HP 48S(X) skins](https://www.hpcalc.org/details/6644), and on
[Christoph's page](https://hp.giesselink.com/emu48.htm) (the Emu48 package contains several
HP48 scripts, among them Emu48's default faceplates and Casey Patterson's scripts). If you are interested in writing new
scripts, get the KML 2.0 documentation from Christoph's page.

Having selected a script, press **OK** to start the emulator. In most cases, when Emu48
fails after pressing OK, you are using an invalid ROM image. While it's running, you can use
*View → Change KML Script…* to change the visual aspect of Emu48.

## Command line

    emu48 [E48file [Port2file]]

The first parameter sets the state file, independent of the last used state file that is
normally opened. The second parameter sets the port 2 file. You can't set a port 2 file
without setting the state file. Both arguments are optional.

On macOS the program is inside the app:

    Emu48.app/Contents/MacOS/Emu48 [E48file [Port2file]]

## Virtual keyboard

There are two ways to use the virtual keyboard of the emulated calculator: the mouse and the
keyboard of your computer.

**Mouse:** the KML script defines buttons with an area where mouse clicks take effect. The
active area is indicated by the hand cursor. Pressing the left mouse button over an active
area presses the virtual button. When the mouse leaves the button area with the left button
still pressed, the virtual button is released automatically. To press more than one key at
once, press the virtual keys with the right mouse button (Control+click on macOS): when you
release the mouse button or leave the key area, the key is still held. To release all held
keys, use the left mouse button again. A single release of a held key isn't possible.

**Keyboard:** the KML script defines which keys of your keyboard press which calculator
keys, so the keyboard usage depends on your KML script. Emu48/SDL maps the keys by their
position on a US keyboard and the letters by their label, so the keyboard mappings of the
KML scripts work with any keyboard layout. *Edit → Copy Stack* and *Paste Stack* use
Control+C and Control+V (Command+C and Command+V on macOS), so these keys can't be used by
the KML script.

## File menu

### New

Creates a new emulation session. You're asked for a KML script where you select the
calculator type and skin to emulate.

### Open

Opens a previously saved emulation session. The emulation continues at the same position
where the session was saved. Loading sessions made with a different ROM revision may destroy
the memory content or cause other unpredictable results. State files are exchangeable with
Emu48 for Windows.

### Save

Saves the current session with its current name.

### Save As

Saves the current session with a new name. You also get this dialog when you exit a new
session without a state file name.

### Close

Closes the current session without closing the emulator.

### Recent files

The last opened state files. Select one to open it.

### Settings

The Settings dialog has three tabs: General, Memory and Peripheral.

#### General

- **Authentic Calculator Speed:** the emulation speed is similar to the real calculator,
  depending on the RATE control register.
- **Enable Virtual LCD Delay:** a better simulation of the 4 color gray scale display.
- **Always On Top:** the emulator window is always the topmost one.
- **Single Instance:** only one instance of the emulator runs. A new instance passes its
  state file to the running instance, which comes to the foreground and changes to this
  state file, and quits. (macOS may refuse to bring the window to the foreground.)
- **Automatically Save Files:** the current state file is saved automatically when you change
  to another state file, but not when you close the emulator.
- **Automatically Save Files On Exit:** the current state file is saved automatically when the
  emulator is closed.
- **Show Load Object Warning:** a warning before *Edit → Load Object…*.
- **Always Show KML Compilation Result:** shows the result of the KML interpreter at every
  KML script load.

*Style:*

- **Show Title:** the window title bar is visible.
- **Show Menu:** the menu bar is visible. Without it, the menu is a context menu in the window
  outside the calculator buttons (right click, or Control+click on macOS).

*Disassembler:* the assembler syntax, **HP Mnemonics** (the standard syntax of HP) or
**Class Mnemonics**. Class (Clarke assembler) was written by Lutz Vieweg in 1991, when HP
had not published their own development tools; the syntax is similar to the AG and STAR
mnemonics of that time, used in many published assembler programs for the HP28S.

#### Memory

*Memory Cards:*

- **Port 1 is Plugged:** a 128 KB RAM card is emulated in port 1. Its content is saved in the
  state file.
- **Port 1 is Writeable:** the RAM card in port 1 is writable, else read-only like with the
  write protect switch of the card.
- **Port 2 is Shared:** when unchecked, only the first instance of Emu48 can use the RAM card
  in port 2. When checked, the first instance gets read and write access and other instances
  get the card write-protected, so you can easily transfer files between two calculators.
  This RAM card is used by both S/SX and G/GX types.
- **Port 2 is Writeable:** the read/write state of the port 2 file. Changing it changes the
  write permission of the file.
- **Port 2 File:** a RAM card of up to 4 MB for an HP48. No card exists by default; create one
  with [MkShared](#mkshared). Copy an existing unpacked port 2 card file into the Emu48
  directory, or enter its full path.

With RAM cards larger than 128 KB in an HP48SX, you only see the first 128 KB of the card.
All HP48GX firmware versions have a bug with a 4 MB RAM card: you always get the message
"Warning: Invalid Card Data" at startup and port 33 is inaccessible. This is not a bug of
the emulator.

Port configuration changes behave like on the real calculator, as if you changed the cards
with the calculator on. This mostly works with an automatic warmstart, but the safest way is
to switch off the emulated calculator first.

#### Peripheral

*Sound:* the sound engine emulates the beeper output ports of the calculator. ROM beep
patches of old KML scripts are not needed any more; Emu48 reports them as an error in the
"KML Script Compilation Result" dialog, so remove them from your KML scripts.

For the sound the calculator must know its CPU strobe frequency. The real calculator
measures it at a cold- or warmstart and stores it in the `=CSPEED` variable, which
determines frequency and duration of the beeps. Emu48 sets the strobe frequency with
`SXCycles` (HP48SX) and `GXCycles` (all other calculators) in the section `[Emulator]` of
`Emu48.ini`; the values are estimated CPU cycles, not the frequency in Hz divided by 16384
like in other emulators. When a state file was made with another value, `=CSPEED` may be
wrong. You discover this by measuring a 10 s beep: a deviation of less than 1 s is ok,
otherwise do a warmstart of the calculator or *Edit → Reset Calculator*. Both measure the
speed again and purge the stack.

- **Volume:** the output volume relative to the system volume.
- **Device:** "Default Device" follows the default output of the system, or choose an output
  device.

*Infrared Printer:* the emulator prints to an HP82240A/B printer simulation over UDP. Enter
the IPv4 address and the port the printer simulator is listening on.

*Serial Ports:*

- **Wire:** the serial device connected to the wire port of the calculator.
- **IR:** the serial device connected to the IR port of the calculator. The IR port only works
  with 2400 baud.

The lists show the devices `/dev/tty.*` (macOS) and `/dev/ttyS*`, `/dev/ttyUSB*`,
`/dev/ttyACM*` (Linux). You can also type any other device path, e.g. a pseudo terminal; an
empty field means disabled. On Linux your user must be a member of the group owning the
device, usually `dialout`. The baud rates 1920, 3840, 7680 and 15360 of the calculator work
when the serial driver supports them (most USB serial adapters).

### Exit

Quits the emulator. What happens with the state file is set in the Settings dialog. If the
current session is "Untitled", you are asked for a file name with *Save As*. If you quit
without a file name, you're asked for a KML script at the next start.

## Edit menu

### Load Object

Only for the HP48SX, HP48GX and HP49G. Loads an HP48 or HP49G binary object to stack level 1.
The object must begin with `HPHP48-x` for an HP48 or `HPHP49-x` for an HP49G, where x is any
alphanumeric character. Without this header the file is loaded as a string. Dropping files on
the emulator window also loads them. Be sure that the calculator isn't busy before doing
this.

### Save Object

Only for the HP48SX, HP48GX and HP49G. Saves the object in stack level 1 as a binary object.

### Copy Screen

Copies the calculator screen as an image to the clipboard.

### Copy Stack

Only for the HP48SX, HP48GX and HP49G. Copies a real number, complex number or string in
stack level 1 to the clipboard; other objects are ignored. The decimal point of real numbers
follows the calculator setting.

### Paste Stack

Only for the HP48SX, HP48GX and HP49G. Pastes the clipboard text to stack level 1. A real
number is stored as a real number object, a complex number as a complex number object,
everything else as a string.

The decimal point and the thousands separator of real numbers are detected automatically;
the thousands separator is removed. Complex numbers must have the form `(a,b)` with decimal
point or `(a;b)` with decimal comma, without thousands separators. The form a+bi is not
supported.

### Reset Calculator

Emulates the reset pin of the CPU.

### Backup

- **Save:** saves the current emulator state into the backup slot, replacing earlier data.
- **Restore:** restores the saved state without question. If you changed the calculator model
  meanwhile, the emulator switches back to the model of the backup.
- **Delete:** deletes the backup.

## View menu

### Change KML Script

Changes the skin of the emulated calculator. Unlike *File → New*, only the scripts for the
same calculator model are shown.

## Tools menu

### Disassembler

A simple disassembler. *Mapping Mode* selects the memory: *Map* disassembles what the CPU
sees, *NCE1* to *NCE3* the memory at a chip select signal linearly (see the table in
[Memory window](#memory-window)). While the debugger is open, the disassembler uses the
mapping of the debugger's memory window. Enter the address in hexadecimal into the *Address (HEX)*
field and press Return. *Next Address* disassembles the next opcode, *Copy Data* copies the selected
lines to the clipboard.

### Debugger

The assembler code debugger of the emulator, see [Debugger](#debugger-1).

### Macro

The keyboard macro recorder.

- **Record:** asks for the macro file, then every key event is recorded with its time.
- **Play:** asks for a macro file and replays it immediately.
- **Stop:** stops recording or replaying.
- **Settings:** the replay speed, *Real* (the recorded speed) or *Manual* (set with the
  slider).

## Help menu

### Help Topics

Shows this manual in the help window: the topics on the left, the chapter of the selected
topic on the right. Links lead to other chapters, **← Back** returns to the previous
chapter, and Escape closes the window. The manual is part of the program; it is also
[online](https://github.com/FabianHassler/Emu48-SDL/blob/main/doc/MANUAL.md).

### About Emu48

Shows the version, the copyright and the license.

## Debugger

*Tools → Debugger…* opens the internal assembly debugger. It was designed to help inspecting
assembler code objects, a part that cannot be handled satisfactorily by the JAZZ package.
Thanks to Mika Heiskanen and all the others supporting this great program.

When the debugger starts, the emulation stops at the current program counter. It continues
after closing the debugger window. Remember that the calculator clock shows the wrong time
then.

**Mouse in Emu48/SDL:** a double click in the lists of the debugger can also be done with
Option+click (macOS), Alt+click (Linux) or the middle mouse button. Context menus open with
the right mouse button or Control+click on macOS.

### Debugger settings

The first menu of the debugger window, *Debugger*, has the item *Debugger Settings…*.

*Disassembler:* HP or Class mnemonics, the same switch as in the main Settings dialog.

*Symbolic:* symbolic debugging with a reference file that converts absolute addresses into
symbolic names. The debugger uses the HP-TOOLS object files (`*.o`) with the supported
entries, which HP provides for the HP48 and HP49 series for linking; for older calculators
you have to make them yourself. Only the output format of HP SASM 3.x is understood. When an
entry has several names, like `=UNROT`, `=3UNROLL` and `=XYZ>ZXY` at `#60FAC`, the last name
in the object file is shown. Without a linker object file, e.g. for the HP28S entry point list
`SUPROM28.A`, create one with `sasm -N SUPROM28.A`.

- **Enable:** symbolic debugging on or off, without removing the reference file.
- **Model:** each calculator model needs its own reference file. The models correspond to the
  KML script; the current model is preselected, and you can switch to the other models to
  enter their files. All file names are saved with OK.
- **Edit field:** the reference file of the selected model.

### Debug menu

- **Run** (F5): continues the emulation under debugger control until a breakpoint. The
  emulation is slower than without the debugger.
- **Run to Cursor** (F6): runs until the address at the cursor; breakpoints may stop earlier.
- **Step Into** (F7): executes one instruction.
- **Step Over** (F8): executes a GOSUB, GOSUBL or GOSBVL as one instruction. Instead of
  stopping behind the GOSUB, which a construction like

        GOSUB   +
        NIBASC  /Hello world/
    +   C=RSTK

  never reaches, the debugger stops when the stack has the same level as before the GOSUB,
  here after the `C=RSTK`.
- **Step Out** (F9): runs until a RTI, RTN, RTNC, RTNCC, RTNNC, RTNSC, RTNSXN or RTNYES above
  the current stack level. Constructions that save space on the hardware stack, like
  `C=RSTK` `PC=C` (no RTN at all) or `C=RSTK` `RSTK=C` `RTN` (the `RSTK=C` looks like a GOSUB),
  stop at the wrong address, so be careful with F9.
- **Break** (F11): stops the emulation at the current program counter.

### Breakpoints menu

- **Set Breakpoint** (F2): toggles a code breakpoint at the cursor in the code window.
- **Edit Breakpoints…:** a sorted list of all breakpoints; checked ones are enabled. *Add*
  adds a new or enables an existing breakpoint, *Delete* deletes the selected ones. Addresses
  above `#FFFFF` are cut after the fifth nibble. A breakpoint is one of
  - *Code:* stops before the opcode at this address is executed
  - *RPL:* stops at the first opcode of the RPL object at this address
  - *Memory Access:* stops before the address is read or written
  - *Memory Read:* stops before the address is read
  - *Memory Write:* stops before the address is written

  A double click on a breakpoint toggles its check box; the space key toggles all selected
  breakpoints.
- **Clear All Breakpoints:** deletes all address breakpoints.
- **NOP3 Code Breakpoints:** stops at a NOP3 opcode. User programs move in memory after a
  garbage collection, so a fixed breakpoint is hard to set; put a NOP3 into your source
  instead. The Saturn CPU has no NOP; SASM.DOC defines NOP3 as opcode `820` (HST=0 0, used by
  HP TOOLS 3.x and the ROM) or `420` (GOC to the next line). The debugger breaks at `420`:

        ASSEMBLE
          NIBASC /HPHP48-E/
        BREAK   MACRO
                CON(3)  #024            NOP3
                ENDM
        RPL
        CODE
                BREAK                   code breakpoint
                GOSBVL  =SAVPTR         save register
                GOSUB   +               problem for step over
                NIBASC  /Hello world/
        +       C=RSTK
                GOVLNG  =GETPTRLOOP
        ENDCODE

- **CODE Object Breakpoints:** stops at the first instruction of every DOCODE object outside
  the ROM. For DOCODE objects in ROM use code breakpoints.
- **RPL Breakpoints:** stops at every instruction after a `PC=(A)` or `PC=(C)`, normally the
  begin of an RPL command. The PC marker is then `-R` instead of `->`.

### Interrupts menu

**Step Over Interrupts:** skips the interrupt handler code. Careful: with interrupts disabled,
all code until interrupts are enabled again belongs to the interrupt handler and can't be
executed in single steps. Breakpoints are still active. To leave an interrupt handler,
check this item, press F7 to stop behind the RTI, and uncheck it again.

### Trace menu

- **Settings…:** the trace file. *Log File* is the output file; without a path it is in the
  Emu48 directory. *File Mode* *New* creates an empty file, *Append* appends (and creates the
  file if needed). *Logging*: *Register* prints the CPU registers (without the write-only
  registers) before each disassembled opcode, *MMU* the MMU registers, *Opcode* the opcode.
- **Enable:** turns the trace on; unchecking it stops the trace and flushes the file.

### Info menu

- **Last Instructions…:** the last 255 executed CPU addresses. The disassembly may be wrong
  because only the address was saved and the memory mapping may have changed. Lines can be
  copied to the clipboard, and the list can be cleared.
- **Profiler…:** the number of CPU cycles and the execution time of the code between the last
  two breakpoints. The cycles are approximate values.
- **Write Only Registers…:** the data written to the write-only display registers, which
  differ from what is read.

### Code window

The disassembled code. The line of the current PC is marked with `->` (or `-R`) between
address and disassembly. Scroll with Up, Page Up, Down and Page Down. When moving to lower
addresses, the debugger doesn't know whether an address is the begin or inside of an opcode,
so the lines may be disassembled wrongly.

Context menu:

- **Go to address…** (G): a hexadecimal address or a symbolic name.
- **Go to PC:** the cursor to the current PC.
- **Set breakpoint** (F2): toggles a code breakpoint at the cursor.
- **Set PC to selection:** sets the PC to the cursor. Careful, this changes the execution
  order.
- **Find:** searches the mapped address area for an address with a PCO (Primitive Code
  Object) header.
- **Previous PCO** / **Next PCO:** the PCO before or behind the first line of the window.

### Register window

The CPU registers, updated when the program stops; changed registers are highlighted. Click a
register to change it; bit registers like CY and Mode change immediately.

### Memory window

The memory in the selected mapping. Move with the arrow keys, Page Up and Page Down; + and −
move the position by one nibble. A double click (only in Map mode) changes the content of the
two addresses, unless the memory is read-only.

Context menu:

- **Go to address…** (G), **Go to PC**, **Go to D0**, **Go to D1**, **Go to Stack** (the
  return address in the top level of the stack).
- **Follow:** the window follows an address. *Follow none* (default) is static, *Follow
  Address Content* follows the pointer stored at an address, *Follow Register PC/D0/D1*
  follows the register. The window is updated after each step.
- **Find…** (F): searches a hexadecimal data sequence, the data of a symbolic name starting
  with `=` (like `=DOCOL`), or an ASCII string, in the area of the current mapping. *Previous*
  and *Next* search further. The strings of the data list are lost when the dialog closes.
- **Mapping:** *Map* (default) shows what the CPU sees, and only here memory can be changed.
  *NCE1/NCE2/CE1/CE2/NCE3* show the memory at this chip select signal linearly, read-only:

  |      | HP38G     | HP39/40G   | HP48S/SX      | HP48G/G+/GX   | HP49G        |
  |------|-----------|------------|---------------|---------------|--------------|
  | NCE1 | ROM 512KB | ROM 1024KB | ROM 256KB     | ROM 512KB     | Flash 2048KB |
  | NCE2 | RAM 32KB  | RAM 128KB  | RAM 32KB      | RAM 32/128KB  | RAM 256KB    |
  | CE1  | nc.       | BS         | Slt1 32/128KB | BS            | BS           |
  | CE2  | nc.       | nc.        | Slt2 32/128KB | Slt1 32/128KB | RAM 128KB    |
  | NCE3 | nc.       | RAM 128KB  | nc.           | Slt2 32KB-4MB | RAM 128KB    |

  (Slt = memory card slot, BS = bank switcher, nc. = not connected)
- **Load Memory Data…:** loads a memory dump file to an address in RAM, packed (two nibbles
  per byte, low nibble first) or unpacked (one nibble per byte). *Auto* detects the format,
  which fails for dumps with only 00 bytes; choose the format manually then.
- **Save Memory Data…:** saves an address area into a packed or unpacked memory dump file.
- **RPL Object Viewer…:** shows the decompiled RPL object at the cursor. To start at an
  address inside the marked two nibbles, move with + and −.

### Stack window

The hardware stack; `1:` is the current return address. A double click shows the address in
the code window. Context menu: *Push* a new element before the selection, *Pop* the selected
element, *Modify* the selected element.

### MMU window

The configuration of the memory controllers. The addresses shown are the first addresses of
the module areas and may differ from the address given to CONFIG: `LC(5) #C0000` `CONFIG`
`LC(5) #98765` `CONFIG` configures a 128 KB module at `#80000`, and the window shows `#80000`.

### Miscellaneous window

The interrupt flag, the 1 ms keyboard handler and the bank switcher latch (only on
calculators with a latch; ignore bit A0, it isn't wired to the six bit latch). Click a value
to change it.

## MkShared

MkShared creates a RAM card file for port 2 of the HP48SX/GX. It is a command line program;
open a terminal in the Emu48 directory and type for example

    ./mkshared 128 SHARED.BIN

to create a 128 KB RAM card with the name `SHARED.BIN`. Valid sizes are 32, 128, 256, 512,
1024, 2048 and 4096 KB; without a file name the card is written to `SHARED.BIN`. An existing
file is replaced without question. Enter the file name in *Settings → Memory → Port 2 File*.

## Differences to Emu48 for Windows

- A double click in the lists of the debugger can also be done with Option+click (macOS),
  Alt+click (Linux) or the middle mouse button.
- On macOS, Control+click works like a right click, and the Command key like the Control key
  in keyboard shortcuts.
- Sub menus are shown as expandable items inside their menu.
- The keys are mapped by their position on a US keyboard and the letters by their label, so
  KML keyboard mappings work with any keyboard layout.
- Serial ports are device paths, see [Peripheral](#peripheral).
- *Help Topics* shows this manual in a help window of the program instead of `Emu48.htm` from
  the Emu48 directory in the web browser.
- The settings are in `Emu48.ini` instead of the registry.
- There is no *Activation Follows Mouse* setting (macOS doesn't let a program in the
  background come to the foreground), no DDE interface and no Convert program.

## License

Emu48 — an HP38G/39G/40G/48SX/48GX/49G emulator\
Copyright © 1995 Sébastien Carlier, Copyright © Christoph Gießelink\
Emu48/SDL: Copyright © 2026 Fabian Hassler

This program is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program;
if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301 USA.
