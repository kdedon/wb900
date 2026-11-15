# Backlog

Queued after gadtools, the native Workbench and the second speed pass. In order:

1. **console.device on windows and a CON: handler.** Shell windows on the Workbench screen, and the Workbench menu's Execute Command. AROS rom/devs/console, fitted to Exec900's dos.library and shell.
2. **commodities.library**, then ClickToFront, AutoPoint, DepthMenu, Blanker and NoCapsLock (AROS workbench/tools/commodities, plain C).
3. **asl.library**: file, font and screen-mode requesters (intuition and gadtools based).
4. **More** (gadtools text viewer), then **Edit** if it isn't Zune-based.
5. **KeyShow, Clock, SysMon, LogView, Calculator.** The AROS versions use Zune, so each is rewritten on intuition and gadtools, keeping AROS's logic where it ports.

Zune (AROS's MUI) is out: too heavy for a real C900 at 6 MHz. Tools are written on intuition and gadtools.

Later: Exchange and the preferences editors (palette, pointer, screen mode, input, time) as native gadtools tools, diskfont.library, and ShowConfig and Help.
