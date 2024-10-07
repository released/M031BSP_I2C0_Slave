# M031BSP_I2C0_Slave
 M031BSP_I2C0_Slave

update @ 2024/10/07

1. init I2C 0 : PB5 : SCL , PB4 : SDA as I2C slave

2. init I2C 1 : PC5 : SCL , PC4 : SDA as I2C master

3. I2C bus need to pull high 

4. use terminal to send command by I2C master , 

- reg : 0x04

- data byte : 4 bytes

5. press digit 1 , below is log and LA capture 

- reg : 0x04

- data [0] : 0x12

- data [1] : 0x34

- data [2] : counter

- data [3] : 0x5A

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/LA_1.jpg)

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/log_1.jpg)


6. press digit 3 , below is log and LA capture 

- reg : 0x04

- data [0] : counter

- data [1] : CRC

- data [2] : 0x5A

- data [3] : 0xA5

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/LA_3.jpg)

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/log_3.jpg)


7. press digit 4 , below is log and LA capture 

- reg : 0x04

- data [0] : 0x0D

- data [1] : 0x1D

- data [2] : CRC

- data [3] : 0x5A

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/LA_4.jpg)

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/log_4.jpg)


8. press digit 5 , below is log and LA capture 

- reg : 0x04

- data [0] : 0x01

- data [1] : 0x1E

- data [2] : CRC

- data [3] : 0x5A

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/LA_5.jpg)

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/log_5.jpg)


9. press digit 6 , below is log and LA capture 

- reg : 0x04

- data [0] : 0x01

- data [1] : 0x1F

- data [2] : CRC

- data [3] : 0x5A

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/LA_6.jpg)

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/log_6.jpg)


10. press digit 9 , below is log and LA capture 

I2C master will write data , with 6 times

- reg : 0x04

- data [0] : 0x56

- data [1] : 0x78

- data [2] : counter

- data [3] : 0x5A

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/LA_9.jpg)

![image](https://github.com/released/M031BSP_I2C0_Slave/blob/main/log_9.jpg)






