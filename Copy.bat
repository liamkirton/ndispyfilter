@echo off

REM copy /y Z:\NdisPyFilter\NdisPyFilterCtrl\Debug\*.exe .
REM copy /y Z:\NdisPyFilter\NdisPyFilterCtrl\Libs\*.dll .
copy /y Z:\NdisPyFilter\NdisPyFilterCtrl\Release\*.exe .

copy /y Z:\NdisPyFilter\NdisPyFilter\*.inf .
REM copy /y Z:\NdisPyFilter\NdisPyFilter\objchk_wlh_x86\i386\*.sys .
copy /y Z:\NdisPyFilter\NdisPyFilter\objfre_wlh_x86\i386\*.sys .

mkdir Filters
copy /y Z:\NdisPyFilter\Filters\*.* .\Filters