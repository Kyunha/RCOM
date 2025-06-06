// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename){
        LLinkLayer config = {
            .serialPort = "/dev/ttyS0",
            .role = LlTx,
            .baudRate = B38400,
            .nRetransmissions = 3,
            .timeout = 10
        };
        int fd = llopen(config);
        llwrite(buffer, sizeof(buffer));
        llclose(TRUE);}

#endif // _APPLICATION_LAYER_H_
