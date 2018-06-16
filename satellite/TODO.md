Textures:
- Talisman architecture anisotropic texture lookup    https://diglib.eg.org/bitstream/handle/10.2312/EGGH.EGGH97.079-087/079-087.pdf?sequence=1&isAllowed=y
- Vectorized path tracing: texture/sampling architecture    http://www.tabellion.org/et/paper17/MoonRay.pdf
- Ray Differentials paper eq 8) I don't get the same thing

- Check that terraprofile hasn't been broken

--- Possible errors ---
(fuck logging system?)

Out of memory
Invalid args


--- moo ---
vs driver:
-> spawn moo
<- rcv command
<- execute command:
	- open/close file
	- goto line



driver API:
query_instances()



best case scenario VSIX.. but I don't have vs express and sending messages is likely as fast if not more.
not sure which one can be faster, but as long as they are both roughly equally fast enough (w/o noticeable differences) i would rather not inject code.


VSIX:
on_startup():
	- Start moo
	- Register keybinding

on_open():
	- focus moo

on_event():
	- execute

Ring-buffer memory mapped file.
read ptr -> updated by driver
write ptr -> updated by moo