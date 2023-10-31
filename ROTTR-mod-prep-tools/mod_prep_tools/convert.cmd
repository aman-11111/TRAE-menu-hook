for %%f in (*.tr2mesh *.tr2pcd) do section2hash.exe %1 "%%f"
move %1 mod_out
for %%f in (mod_out\*.tr2mesh mod_out\*.tr2pcd) do cdrm.exe "%%f"