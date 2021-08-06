set HOME_PATH=%~dp0

cd External/

for /r %%f in (*.bat) do (
if %%~nxf==pb_genbuild.bat (
	cd %%~dpf
	call %%f
	cd %HOME_PATH%
	)
)

cd ../