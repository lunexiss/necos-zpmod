cd C:\hlsdkbuild\zpmod
REM rmdir /s /q build
REM mkdir build
cd build

cmake .. -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DBUILD_SERVER=ON ^
  -DBUILD_CLIENT=OFF ^
  -DXASH_WIN32=ON ^
  -DXASH_X86=ON

cmake --build . -j

cp "C:\hlsdkbuild\zpmod\build\dlls\zpserver.dll" "C:\Program Files (x86)\Steam\steamapps\common\Half-Life\valve\dlls"

set /p START=do you want to start the game (y/n)
if /i "%START%"=="y" (
    "C:\Program Files (x86)\Steam\steamapps\common\Half-Life\xash3d.exe" +exec listenserver.cfg +coop 0 +map crossfire +deathmatch 1 -dev 5 -log
) else (
    echo skipping game launch
)