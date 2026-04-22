@{
    ProjectPath = 'net/MDK/network.uvprojx'
    KeilTarget = 'Network'
    HexFilePath = 'net/OBJ/network.hex'

    JLink = @{
        Device = 'GD32F407VG'
        Interface = 'SWD'
        SpeedKHz = 4000
        GdbPort = 3331
        SwoPort = 3332
        TelnetPort = 3333
        RttTelnetPort = 19021
    }

    Serial = @{
        BaudRate = 115200
        DataBits = 8
        Parity = 'None'
        StopBits = 'One'
        DtrEnable = $false
        RtsEnable = $false
    }
}