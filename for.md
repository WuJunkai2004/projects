## for - cmd
　　author ： Wu Junkai
#### 1.迭代器
　　这是 for 常用功能，以遍历(...)里的数据为基础，数据以空格或逗号隔开。  
###### exmaple  
    for %%i in ( A,B,C ) do set /P =%%i <nul
###### input
    A B C

#### 2.循环
　　这也是 for 的常用功能，可进行循环。  
###### example
    for /L %%i in (1,1,5) do set /P =%%i <nul
###### input
    1 2 3 4 5

#### 3.字符串
　　字符串是 for 最强大也也是最复杂的功能，可以分割一串字符、读取文件等。
