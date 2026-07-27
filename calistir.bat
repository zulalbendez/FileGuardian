@echo off
chcp 65001 >nul
echo ============================================
echo   Dosya Etiketleme Araci
echo ============================================
echo.
set /p klasor="Taranacak klasorun tam yolunu yapistir ve Enter'a bas: "
echo.
tagger.exe "%klasor%"
echo.
pause