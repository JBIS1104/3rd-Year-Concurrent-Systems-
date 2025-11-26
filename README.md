# 3rd-Year-Concurrent-Systems-
3rd year 1st Semester subject Concurrent systems Coursework. 
Make sure your C/C++ compiler is installed

$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path

& "C:\msys64\ucrt64\bin\g++.exe" -static -std=c++20 -DASIO_STANDALONE -D_WIN32_WINNT=0x0A00 -I . Race.cpp -o Sender.exe -lws2_32 -lmswsock

.\Sender.exe



& "C:\msys64\ucrt64\bin\g++.exe" -static -std=c++20 -DASIO_STANDALONE -D_WIN32_WINNT=0x0A00 -I . 10936972_Park_Task5_Benchmark.cpp -o Benchmark.exe -lws2_32 -lmswsock
