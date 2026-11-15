# Backlog

After gadtools, the native Workbench and the second speed pass. Done:

1. **console.device on windows and a CON: handler**: Shell windows and the Workbench menu's Execute Command. AROS rom/devs/console on Exec900's dos.library and shell.
2. **commodities.library**, ClickToFront, AutoPoint, DepthMenu, Blanker, NoCapsLock (AROS workbench/tools/commodities).
3. **asl.library**: file, font and screen-mode requesters.
4. **More** (gadtools text viewer) and **Edit** (JanoEditor).
5. **KeyShow, Clock, SysMon, LogView, Calculator**: AROS's use Zune, so rewritten on intuition and gadtools, keeping AROS logic where it ports.
6. **ScreenMode** (SYS:Prefs): the Workbench mode in ENV:Sys/screenmode.prefs and the 1.x look; Shift held at Workbench start ignores the saved mode.

Zune (AROS's MUI) is out: too heavy at 6 MHz. Tools use intuition and gadtools.

Next: Exchange and the preferences editors (palette, pointer, input, time) as native gadtools tools, diskfont.library, ShowConfig and Help.

Decided against (2026-11-15): LR graphics. The LR card is MDA-like (HD6545, 720x350, mono with intensity, fixed character ROM); CGA-style text tricks would give at best about 160x175 in 3 shades, too little for Workbench.
