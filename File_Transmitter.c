#include <windows.h> // Windows API için gerekli kütüphane
#include <stdio.h>
#include <stdint.h>

#define CHUNK_SIZE 1024

#define SOF_BYTE 0xAA
#define EOF_BYTE 0xEE

// #define APP_BIN_FILE "Application v1.1.bin" // 0x0806000
// #define APP_BIN_FILE "Application v0.1.bin" // 0x0804000

#define APP_BIN_FILE "app.bin" // 0x0806000

enum response
{
    READY_TO_UPLOAD = 0x01,
    RECEIVE_OK,
    RECEIVE_FULL,
    RECEIVE_ERR
};

void pack_frame(uint8_t *frame, uint8_t *data, uint16_t pkg_no, uint16_t len);
uint8_t split_file_to_chunks(uint8_t data[][CHUNK_SIZE]);
void send_upload_frame(uint8_t app, uint16_t chunk_piece, HANDLE hSerial);
void send_end_frame(HANDLE hSerial);
uint8_t receive_frame(HANDLE hSerial);

int main()
{
    uint8_t *buf;
    uint8_t data[128][CHUNK_SIZE] = {{'\0'}, {'\0'}};
    uint8_t frame[128][CHUNK_SIZE + 10] = {0};
    uint16_t chunk_nbr = 0;

    DWORD bytes_written;
    HANDLE hSerial = CreateFile("\\\\.\\COM17", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        printf("Port can't open!");
        return 1; // Port açılamazsa çık
    }
    // Seri port ayarları
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    dcbSerialParams.BaudRate = 500000;     // Baud hızı
    dcbSerialParams.ByteSize = 8;          // Veri uzunluğu
    dcbSerialParams.StopBits = ONESTOPBIT; // Durak bitleri
    dcbSerialParams.Parity = NOPARITY;     // Eşlik bitleri

    SetCommState(hSerial, &dcbSerialParams); // Ayarları uygula

    // Seri port üzerinden veri gönderme

    chunk_nbr = split_file_to_chunks(data);

    // Pack and send data =======================================================
    send_upload_frame(2, chunk_nbr, hSerial);

    if (receive_frame(hSerial) == READY_TO_UPLOAD)
    {
        if (chunk_nbr != 0)
        {
            for (int i = 0; i <= chunk_nbr; i++)
            {
                pack_frame(frame[i], data[i], i, sizeof(data[i]));

                WriteFile(hSerial, frame[i], sizeof(frame[i]) + 1, &bytes_written, NULL); // Veriyi gönder
                // printf(" Data sent:%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-...-%x\n", frame[i][0], frame[i][1], frame[i][2], frame[i][3], frame[i][4], frame[i][5], frame[i][6], frame[i][7], frame[i][8], frame[i][9], frame[i][sizeof(data[0]) + 10]);
                if (receive_frame(hSerial) == RECEIVE_OK)
                {
                    printf("[%d/%d]\t%d bytes\t", i, chunk_nbr, sizeof(data[i]));
                }
                else if (receive_frame(hSerial) == RECEIVE_ERR)
                {
                    printf("Bootloader can't receive\n");
                    i = chunk_nbr;
                }
                else
                {
                    printf("Frame error!\n");
                }
            }
        }
        else
            printf("File splitting error!\n");
    }
    else if (receive_frame(hSerial) == RECEIVE_ERR)
    {
        printf("Receive err frame!!\n");
    }
    else
    {
        printf("Can't communicate with bootloader!\n");
    }

    send_end_frame(hSerial);

    if (receive_frame(hSerial) == RECEIVE_FULL)
    {
        printf("app.bin file successfully sent!!!\n");
    }
    else if (receive_frame(hSerial) == RECEIVE_ERR)
    {
        printf("Error at ending!\n");
    }
    // ==========================================================================
    CloseHandle(hSerial);
    // Seri portu kapat

    return 0;
}

void pack_frame(uint8_t *frame, uint8_t *data, uint16_t pkg_no, uint16_t len)
{
    // uint8_t frame[CHUNK_SIZE + 10] = {0};
    uint32_t crc_32 = 200;
    // printf("len:%d\n", len);
    frame[0] = SOF_BYTE;
    // uint16_t pkg_no;
    frame[1] = 1;
    frame[2] = (uint8_t)((pkg_no >> 8) & 0xFF);
    frame[3] = (uint8_t)(pkg_no & 0xFF);

    frame[4] = (uint8_t)((len >> 8) & 0xFF);
    frame[5] = (uint8_t)(len & 0xFF);

    for (int i = 0; i < len; i++)
    {
        frame[i + 6] = data[i];
    }
    frame[len + 6] = (uint8_t)((crc_32 >> 24) & 0xFF);
    frame[len + 7] = (uint8_t)((crc_32 >> 16) & 0xFF);
    frame[len + 8] = (uint8_t)((crc_32 >> 8) & 0xFF);
    frame[len + 9] = (uint8_t)(crc_32 & 0xFF);

    frame[len + 10] = EOF_BYTE;
    // printf("%s\n", frame);
    return;
}

uint8_t split_file_to_chunks(uint8_t data[][CHUNK_SIZE])
{
    FILE *file;

    uint16_t chunk_piece;
    uint16_t last_chunk_len;
    uint64_t file_size = 0;
    uint64_t br = 0;

    file = fopen(APP_BIN_FILE, "rb");

    if (file == NULL)
    {
        printf("File can't open!!");
        return 0;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("File len: %d bytes\n", file_size);

    chunk_piece = file_size / CHUNK_SIZE;
    last_chunk_len = file_size % CHUNK_SIZE;
    printf("Chunk piece: %d chunks\n", chunk_piece);
    printf("Last chunk: %d bytes\n\n", last_chunk_len);

    for (int i = 0; i < chunk_piece; i++)
    {
        br = fread(data[i], 1, CHUNK_SIZE, file);
        // printf("\nChunk %d: ", i);
        // for (int j = 0; j < CHUNK_SIZE; j++)
        // {
        //     printf("%d ", data[i][j]);
        // }
        // printf("\n");
        if (br != CHUNK_SIZE)
        {
            printf("File read error! %d bytes been read\n", br);
            fclose(file);
            return 0;
        }
        // printf("Chunk [%d]: %s\n", i, data[i]);
    }

    br = fread(data[chunk_piece], 1, last_chunk_len, file);
    if (br != last_chunk_len)
    {
        printf("File read error! (last chunk)\n");
        fclose(file);
        return 0;
    }
    // printf("Chunk [%d]: %s\n", chunk_piece, data[chunk_piece]);

    printf("File read and split success..\n");
    fclose(file);
    return chunk_piece;
}

void send_upload_frame(uint8_t app, uint16_t chunk_piece, HANDLE hSerial)
{
    int bw;
    uint8_t frame[6] = {SOF_BYTE,
                        0x02,
                        app,
                        (uint8_t)((chunk_piece >> 8) & 0xFF),
                        (uint8_t)(chunk_piece & 0xFF),
                        EOF_BYTE};
    WriteFile(hSerial, frame, sizeof(frame) + 1, &bw, NULL); // Veriyi gönder
    if (bw == 0)
    {
        printf("Upload frame sent err!!\n");
    }
}

void send_end_frame(HANDLE hSerial)
{
    int bw;
    uint8_t frame[6] = {SOF_BYTE,
                        0x03,
                        EOF_BYTE};
    WriteFile(hSerial, frame, sizeof(frame) + 1, &bw, NULL); // Veriyi gönder
    if (bw == 0)
    {
        printf("End frame sent err!!");
    }
}

uint8_t receive_frame(HANDLE hSerial)
{
    DWORD bytes_read;
    uint8_t buffer[4] = {0}; // Buffer to store received frame

    ReadFile(hSerial, buffer, 4, &bytes_read, NULL);

    // printf("Received data: %x,%x,%x,%x\n", buffer[0], buffer[1], buffer[2], buffer[3]);

    if (bytes_read > 0)
    {
        if (buffer[0] == SOF_BYTE && buffer[3] == EOF_BYTE) // Check for SOF and EOF
        {
            // printf("Response:%x\n", buffer[2]);
            switch (buffer[2])
            {
            case READY_TO_UPLOAD:
                printf("Ready to Upload!\n");
                break;
            case RECEIVE_OK:
                printf("Receive OK!\n");
                break;
            case RECEIVE_FULL:
                printf("Receive FULL!\n");
                break;
            case RECEIVE_ERR:
                printf("Receive ERROR!\n");
                break;
            }
            return buffer[2];
        }
        else
        {
            printf("Invalid frame received\n");
        }
    }
    if (bytes_read == 0)
    {
        printf("Can't receive any data!\n");
    }
}
