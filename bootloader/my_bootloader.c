
/**
 * @bieaf YModem升级
 *
 * @param none
 * @return none
 */
void ymodem_fun(void)
{
    int i;
    if (Get_state() == TO_START)
    {
        send_command(CCC);
        HAL_Delay(1000);
    }
    if (Rx_Flag) // Receive flag
    {
        Rx_Flag = 0; // clean flag

        /* 拷贝 */
        temp_len = Rx_Len;
        for (i = 0; i < temp_len; i++)
        {
            temp_buf[i] = Rx_Buf[i];
        }

        switch (temp_buf[0])
        {
        case SOH: ///< 数据包开始
        {
            static unsigned char data_state = 0;
            static unsigned int app2_size = 0;
            if (Check_CRC(temp_buf, temp_len) == 1) ///< 通过CRC16校验
            {
                if ((Get_state() == TO_START) && (temp_buf[1] == 0x00) && (temp_buf[2] == (unsigned char)(~temp_buf[1]))) ///< 开始
                {
                    printf("> Receive start...\r\n");

                    Set_state(TO_RECEIVE_DATA);
                    data_state = 0x01;
                    send_command(ACK);
                    send_command(CCC);

                    /* 擦除App2 */
                    Erase_page(Application_2_Addr, 40);
                }
                else if ((Get_state() == TO_RECEIVE_END) && (temp_buf[1] == 0x00) && (temp_buf[2] == (unsigned char)(~temp_buf[1]))) ///< 结束
                {
                    printf("> Receive end...\r\n");

                    Set_Update_Down();
                    Set_state(TO_START);
                    send_command(ACK);
                    HAL_NVIC_SystemReset();
                }
                else if ((Get_state() == TO_RECEIVE_DATA) && (temp_buf[1] == data_state) && (temp_buf[2] == (unsigned char)(~temp_buf[1]))) ///< 接收数据
                {
                    printf("> Receive data bag:%d byte\r\n", data_state * 128);

                    /* 烧录程序 */
                    WriteFlash((Application_2_Addr + (data_state - 1) * 128), (uint32_t *)(&temp_buf[3]), 32);
                    data_state++;

                    send_command(ACK);
                }
            }
            else
            {
                printf("> Notpass crc\r\n");
            }
        }
        break;
        case EOT: // 数据包开始
        {
            if (Get_state() == TO_RECEIVE_DATA)
            {
                printf("> Receive EOT1...\r\n");

                Set_state(TO_RECEIVE_EOT2);
                send_command(NACK);
            }
            else if (Get_state() == TO_RECEIVE_EOT2)
            {
                printf("> Receive EOT2...\r\n");

                Set_state(TO_RECEIVE_END);
                send_command(ACK);
                send_command(CCC);
            }
            else
            {
                printf("> Receive EOT, But error...\r\n");
            }
        }
        break;
        }
    }
}