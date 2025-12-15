@echo off
cd /d "%~dp0"
echo Compiling shaders...
for %%f in (*.vert) do (
    echo Compiling %%f
    glslc "%%f" -o "%%f.spv"
)
for %%f in (*.frag) do (
    echo Compiling %%f
    glslc "%%f" -o "%%f.spv"
)
echo Done.
