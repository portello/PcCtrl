#include "commHandler.h"

#include <QDebug>
#include <QTime>
#include <QtCore>

//////////////////////////////////////////////////////////////////////////////////////*/
///    ##     ## ####    ######## ##     ## ########  ########    ###    ########     */
///    ##     ##  ##        ##    ##     ## ##     ## ##         ## ##   ##     ##    */
///    ##     ##  ##        ##    ##     ## ##     ## ##        ##   ##  ##     ##    */
///    ##     ##  ##        ##    ######### ########  ######   ##     ## ##     ##    */
///    ##     ##  ##        ##    ##     ## ##   ##   ##       ######### ##     ##    */
///    ##     ##  ##        ##    ##     ## ##    ##  ##       ##     ## ##     ##    */
///     #######  ####       ##    ##     ## ##     ## ######## ##     ## ########     */
//////////////////////////////////////////////////////////////////////////////////////*/
ComHandler::~ComHandler()
{

}

ComHandler::ComHandler(QIODevice* _ioDev, QObject *parent) : QObject(parent), f_ioDev(_ioDev),
    f_status(ComHandlerStatus::STOPPED)
{
    assert(f_ioDev != nullptr);
    connect(f_ioDev, &QIODevice::readyRead, this, &ComHandler::readyRead);
}

void ComHandler::start()
{
    this->setParent(nullptr);
    this->moveToThread(&f_thread);
    f_ioDev->moveToThread(&f_thread);
    f_status = ComHandlerStatus::IDLE;

    f_thread.start();
}

bool ComHandler::isOpen()
{
    return f_ioDev != nullptr && f_ioDev->isOpen();
}

void ComHandler::threadExit()
{
    f_thread.wait();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
///    ########     ###     ######  ##    ##    ######## ##     ## ########  ########    ###    ########     */
///    ##     ##   ## ##   ##    ## ##   ##        ##    ##     ## ##     ## ##         ## ##   ##     ##    */
///    ##     ##  ##   ##  ##       ##  ##         ##    ##     ## ##     ## ##        ##   ##  ##     ##    */
///    ########  ##     ## ##       #####          ##    ######### ########  ######   ##     ## ##     ##    */
///    ##     ## ######### ##       ##  ##         ##    ##     ## ##   ##   ##       ######### ##     ##    */
///    ##     ## ##     ## ##    ## ##   ##        ##    ##     ## ##    ##  ##       ##     ## ##     ##    */
///    ########  ##     ##  ######  ##    ##       ##    ##     ## ##     ## ######## ##     ## ########     */
/////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
void ComHandler::readyRead()
{
    if (f_status == ComHandlerStatus::RECEIVE || f_status == ComHandlerStatus::SEND)
        return;

    QString str(f_ioDev->readAll());
    int idx;
    while ((idx = str.indexOf('\n')) != -1)
    {
        f_lineBuff.append(str.left(idx));
        emit S_lineReceived(f_lineBuff);
        f_lineBuff.clear();
        str.remove(0, idx+1);
    }
    f_lineBuff.append(str);
}

void ComHandler::close()
{
    disconnect(f_ioDev, &QIODevice::readyRead, this, &ComHandler::readyRead);
    f_ioDev->close();
    f_ioDev->deleteLater();
    f_ioDev = nullptr;
    f_thread.exit(0);
}

void ComHandler::sendCmd(QString line)
{
    if (!f_thread.isRunning())
        return ;

    if (f_status == ComHandlerStatus::IDLE)
    {
        if (!line.endsWith('\n'))
            line += '\n';
        f_ioDev->write(line.toLatin1());
    }
}

int ComHandler::sendBuffer(quint8 *tx_buf, int tx_buf_len)
{
    f_ioDev->write(QByteArray(reinterpret_cast<char *>(tx_buf), tx_buf_len), tx_buf_len);

    return(0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
///    ######## #### ##       ########    ######## ########     ###    ##    ##  ######  ######## ######## ########     */
///    ##        ##  ##       ##             ##    ##     ##   ## ##   ###   ## ##    ## ##       ##       ##     ##    */
///    ##        ##  ##       ##             ##    ##     ##  ##   ##  ####  ## ##       ##       ##       ##     ##    */
///    ######    ##  ##       ######         ##    ########  ##     ## ## ## ##  ######  ######   ######   ########     */
///    ##        ##  ##       ##             ##    ##   ##   ######### ##  ####       ## ##       ##       ##   ##      */
///    ##        ##  ##       ##             ##    ##    ##  ##     ## ##   ### ##    ## ##       ##       ##    ##     */
///    ##       #### ######## ########       ##    ##     ## ##     ## ##    ##  ######  ##       ######## ##     ##    */
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/


#define MAXERROR				   5

#define DATA_SIZE            1024
#define PACKET_SIZE          1032
#define SOH_IDX                 0
#define TYPE_IDX                1
#define PACKET_NUM_IDX          2
#define DATA_IDX                6
#define CHKSUM_IDX           (PACKET_SIZE - 1)

#define ANSW_PACKET_SIZE	       8
#define ANSW_SOH_IDX		       0
#define ANSW_ACK_IDX		       1
//#define ANSW_PACK_NUM_IDX          2
#define ANSW_CHKSUM_IDX         (ANSW_PACKET_SIZE - 1)

#define ACK             'A'
#define NAK             'K'
#define SOH             'S'

#define TYPE_HEADER              'H'
#define TYPE_DATA                'D'
#define TYPE_END                 'E'
#define TYPE_ERR                 'R'

#define STATE_W_START               0
#define STATE_F_PACKET              1
#define STATE_PACKET                2
#define STATE_ERR                   3
#define STATE_ACK                   4
#define STATE_L_PACKET              5
#define STATE_END                   6

#define STATE_START                7

#define START_TIMEOUT            1000
#define FAST_ACK_TIMEOUT            0
#define ACK_TIMEOUT              3000
#define GET_TIMEOUT              5000


#define M(MACRO) do {MACRO} while(0)
#define INIT_HEADER_PACKT(buffer, count, len)   M(buffer[SOH_IDX] = SOH;buffer[TYPE_IDX]=TYPE_HEADER;*(reinterpret_cast<quint32*>(buffer + PACKET_NUM_IDX)) = (count); *(reinterpret_cast<quint32*>(buffer + DATA_IDX)) = (len); *(reinterpret_cast<quint32*>(buffer + DATA_IDX + 4)) = 0;)
#define INIT_DATA_PACKT(buffer, count)          M(buffer[SOH_IDX] = SOH;buffer[TYPE_IDX]=TYPE_DATA;*(reinterpret_cast<quint32*>(buffer + PACKET_NUM_IDX)) = (count);buffer[CHKSUM_IDX] = checkSum(buffer, PACKET_SIZE - 1);)
#define INIT_END_PACKT(buffer, count, CS)       M(buffer[SOH_IDX] = SOH;buffer[TYPE_IDX]=TYPE_END;*(reinterpret_cast<quint32*>(buffer + PACKET_NUM_IDX)) = (count); *(reinterpret_cast<quint32*>(buffer + DATA_IDX)) = (CS);buffer[CHKSUM_IDX] = checkSum(buffer, PACKET_SIZE - 1);)
#define INIT_ACK_PACKT(buffer, count)           M(buffer[ANSW_SOH_IDX] = SOH;buffer[ANSW_ACK_IDX]=ACK;*(reinterpret_cast<quint32*>(buffer + PACKET_NUM_IDX)) = (count); buffer[ANSW_CHKSUM_IDX] = checkSum(buffer, ANSW_PACKET_SIZE - 1);)
#define GET_PACKET_NUM(buffer)                  (*(reinterpret_cast<quint32*>(buffer + PACKET_NUM_IDX)))


static uchar checkSum(quint8 *buffer, int len)
{
    quint8    chk = 0;

    for (int i = 0; i < len; i++)
    {
        chk += buffer[i];
    }
    return chk;
}

int ComHandler::getBuffer(quint8 *buff, int buffSize, int tmout)
{
    QTime   time;

    time.start();
    while (f_ioDev != nullptr && buffSize > 0)
    {
        if (f_ioDev->bytesAvailable() >= buffSize)
        {
            quint64 size = f_ioDev->read(reinterpret_cast<char*>(buff), qMin(static_cast<int>(f_ioDev->bytesAvailable()), buffSize));
            buff += size;
            buffSize -= size;
        }
        else
        {
            QCoreApplication::processEvents();
            QThread::usleep(1000);
        }
        if (buffSize > 0 && time.elapsed() > tmout)
            return -1;
    }

    if (f_ioDev == nullptr)
        return -1;
    return(0);
}

int ComHandler::getPacket(int timeout, int packetSize, quint8 *buffer)
{
    if(getBuffer(buffer, packetSize, timeout) != 0)
        return -1;
    if (buffer[SOH_IDX] != SOH)
        return -2;
    if (buffer[TYPE_IDX] == TYPE_ERR)
        return -3;
    if (buffer[packetSize - 1] != checkSum(buffer, packetSize - 1))
        return -4;

    return 0;
}

int ComHandler::getFirstPacket(int timeout, int packetSize, quint8 *buffer)
{
    QTime   time;

    time.start();
    while (time.elapsed() < timeout)
    {
        if (getBuffer(buffer, 1, timeout) != 0)
            return -1;
        if (buffer[0] == SOH)
            break;
//        qDebug() << "getFirstPacket: " << buffer[0];
    }
    if(getBuffer(buffer+1, packetSize-1, timeout) != 0)
        return -2;
    if (buffer[SOH_IDX] != SOH)
        return -3;
    if (buffer[TYPE_IDX] == TYPE_ERR)
        return -4;
    if (buffer[packetSize - 1] != checkSum(buffer, packetSize - 1))
        return -5;

    return 0;
}

int ComHandler::getAck(quint32 *lastAckPacket, bool fast)
{
    uint8_t     buffer[ANSW_PACKET_SIZE];
    int         timeout = fast ? FAST_ACK_TIMEOUT : ACK_TIMEOUT;

    if (!getPacket(timeout, ANSW_PACKET_SIZE, buffer))
    {
        if (*lastAckPacket+1 == GET_PACKET_NUM(buffer))
            (*lastAckPacket)++;
        else
            return -1;
    }
    else
    {
        return fast ? 0 : 1;
    }

    return 0;
}


int ComHandler::fileSend(QDataStream &stream, int len, const QString &fileName, bool fastMode)
{
    int         state = STATE_START;
    int         errors = 0;
    quint32     packetNum = 1;
    quint32     totalPacket = len / 1024 + 1;
    quint32     lastAckPacket = 0;
    quint32     lastProgress = 110;
    quint8      buffer[PACKET_SIZE];
    int         byteSent = 0;
    quint32     fileCheckSum = 0;

    do
    {
        switch (state)
        {
            case STATE_START:
//                qDebug() << "STATE_START";
                while (errors < MAXERROR)
                {
                    if (getFirstPacket(START_TIMEOUT, ANSW_PACKET_SIZE, buffer) == 0)
                    {
                        lastAckPacket = GET_PACKET_NUM(buffer);
                        if (buffer[ANSW_ACK_IDX] == ACK && buffer[ANSW_SOH_IDX] == SOH)
                        {
                            state = STATE_F_PACKET;
                            break;
                        }
                        else
                        {
                            errors++;
//                            qDebug() << "STATE_START: ACK error " << errors;
                        }
                    }
                    else
                    {
                        errors++;
//                        qDebug() << "STATE_START: TimeOut " << errors << " ret=" << ret;
                    }
                }
                if(errors >= MAXERROR)
                    state = STATE_ERR;
                break;
            case STATE_F_PACKET:
//                qDebug() << "STATE_F_PACKET";
                packetNum = 1;
                byteSent = 0;
                fileCheckSum = 0;
                lastAckPacket = 0;
                memset(buffer, 0, sizeof(buffer));
                INIT_HEADER_PACKT(buffer, packetNum++, len);
                strncpy(reinterpret_cast<char *>(&buffer[DATA_IDX + 16]), fileName.toLatin1().data(), DATA_SIZE - 16);
                buffer[CHKSUM_IDX] = checkSum(buffer, PACKET_SIZE - 1);
                if (sendBuffer(buffer, PACKET_SIZE) != 0)
                    state = STATE_ERR;
                else
                    state = STATE_PACKET;
                break;
            case STATE_PACKET:
//                qDebug() << "STATE_PACKET packetNum=" << packetNum+1;
                memset(buffer, 0, sizeof(buffer));
                byteSent += stream.readRawData(reinterpret_cast<char*>(&buffer[DATA_IDX]), DATA_SIZE);
                INIT_DATA_PACKT(buffer, packetNum++);
                fileCheckSum += buffer[CHKSUM_IDX];
                if (sendBuffer(buffer, PACKET_SIZE) != 0)
                    state = STATE_ERR;
                else if (byteSent >= len)
                    state = STATE_ACK;
                else
                    state = STATE_PACKET;
                if (getAck(&lastAckPacket, fastMode) != 0)
                    state = STATE_ERR;
                if (lastProgress != (lastAckPacket * 100)/totalPacket)
                {
                    lastProgress = (lastAckPacket * 100)/totalPacket;
                    emit S_transferProgress(lastProgress);
                }
                break;
            case STATE_ACK:
//                qDebug() << "STATE_ACK LastAckPacket=" << lastAckPacket << "packetNum=" << packetNum;
                while (lastAckPacket < packetNum - 1)
                {
                    if (getAck(&lastAckPacket, fastMode) != 0)
                        state = STATE_ERR;
                    emit S_transferProgress((lastAckPacket * 100)/totalPacket);
                    if (state == STATE_ERR)
                        break;
                }
                state = STATE_L_PACKET;
                break;
            case STATE_L_PACKET:
//                qDebug() << "STATE_L_PACKET";
                memset(buffer, 0, sizeof(buffer));
                INIT_END_PACKT(buffer, packetNum++, fileCheckSum);
                if (sendBuffer(buffer, PACKET_SIZE) != 0)
                    state = STATE_ERR;
                else
                    state = STATE_END;
                emit S_transferProgress(100);
                f_ioDev->waitForBytesWritten(500);
                break;
            case STATE_END:
//                qDebug() << "STATE_END";
                if (getPacket(ACK_TIMEOUT, ANSW_PACKET_SIZE, buffer))
                    return -1;
                return 0;
            case STATE_ERR:
                qDebug() << "STATE_ERR";
                // Invio pacchetto di chiusura
                memset(buffer, 0, sizeof(buffer));
                buffer[SOH_IDX] = SOH;
                buffer[TYPE_IDX] = TYPE_ERR;
                sendBuffer(buffer, PACKET_SIZE);

                while (getBuffer(buffer, 1, ACK_TIMEOUT) == 0)
                    ;
                return -1;
        }
    } while (1);
}

int ComHandler::fileReceive(QDataStream &stream)
{
    int         state = STATE_START;
    uint        count = 0;
    quint32    	packetNum = 0;
    quint8      buffer[PACKET_SIZE];
    quint64     byteReceived = 0;
    quint64     fileLen = 0;
    quint32     fileCheckSum = 0;
    int         progress = -1;
    int         errors = 0;

     do
     {
         switch (state)
         {
             case STATE_START:
//                qDebug() << "STATE_START";
                 if (getBuffer(buffer, 2, START_TIMEOUT) != 0)
                 {
                     errors++;
                     if(errors >= MAXERROR)
                         state = STATE_ERR;
                 }
                 else
                 {
                   if (buffer[0] == '<')
                       state = STATE_F_PACKET;
                   else
                     errors++;
                 }

                 count = 0;
                 packetNum = 0;
                 byteReceived = 0;
                 fileLen = 0;
                 fileCheckSum = 0;
                 memset(buffer, 0, sizeof(buffer));
                 INIT_ACK_PACKT(buffer, 0);
                 if (sendBuffer(buffer, ANSW_PACKET_SIZE) != 0)
                     state = STATE_ERR;
                 else
                     state = STATE_F_PACKET;
                 break;
             case STATE_F_PACKET:
//                 qDebug() << "STATE_F_PACKET";
                 fileCheckSum = 0;
                 if (getPacket(GET_TIMEOUT, PACKET_SIZE, buffer))
                 {
                     state = STATE_ERR;
                     break;
                 }
                 fileLen = *(reinterpret_cast<quint32*>(buffer + DATA_IDX));
                 state = STATE_PACKET;
                 count = packetNum = GET_PACKET_NUM(buffer);
                 memset(buffer, 0, sizeof(buffer));
                 INIT_ACK_PACKT(buffer, packetNum);
                 if (sendBuffer(buffer, ANSW_PACKET_SIZE) != 0)
                     state = STATE_ERR;
//                 if (fileLen == 0)
//                     state = STATE_ERR;
                 break;
             case STATE_PACKET:
                 count++;
//                 qDebug() << "STATE_PACKET " << count;
                 if (getPacket(GET_TIMEOUT, PACKET_SIZE, buffer))
                 {
                     state = STATE_ERR;
                     break;
                 }
                 fileCheckSum += buffer[CHKSUM_IDX];
                 packetNum = GET_PACKET_NUM(buffer);
                 stream.writeRawData(reinterpret_cast<char*>(&buffer[DATA_IDX]), fileLen - byteReceived > DATA_SIZE ? DATA_SIZE : fileLen - byteReceived);
                 byteReceived += DATA_SIZE;
                 if (count != packetNum)
                 {
//                     qDebug() << "STATE_PACKET, received packetNum " << packetNum;
                     state = STATE_ERR;
                     break;
                 }
                 {
                 int progress1 = progress;
                 if (fileLen > 0)
                    progress = (byteReceived * 100) / fileLen;
                 if (progress1 != progress)
                    emit S_transferProgress(progress);
                 }
                 memset(buffer, 0, sizeof(buffer));
                 INIT_ACK_PACKT(buffer, packetNum);
                 if (sendBuffer(buffer, ANSW_PACKET_SIZE) != 0)
                 {
                     state = STATE_ERR;
                     break;
                 }
                 if (byteReceived >= fileLen)
                     state = STATE_L_PACKET;
                 else
                     state = STATE_PACKET;
                 break;
             case STATE_L_PACKET:
                 count++;
//                 qDebug() << "STATE_L_PACKET " << count;
                 if (getPacket(GET_TIMEOUT, PACKET_SIZE, buffer))
                 {
                     state = STATE_ERR;
                     break;
                 }
                 if (fileCheckSum != *(reinterpret_cast<quint32*>(buffer + DATA_IDX)))
                 {
                     state = STATE_ERR;
                     break;
                 }
                 packetNum = *(reinterpret_cast<quint32*>(buffer + DATA_IDX));
                 memset(buffer, 0, sizeof(buffer));
                 INIT_ACK_PACKT(buffer, packetNum);
                 if (sendBuffer(buffer, ANSW_PACKET_SIZE) != 0)
                 {
                     state = STATE_ERR;
                     break;
                 }
                 return 0;
             case STATE_ERR:
                 qDebug() << "STATE_ERR";
                 // Invio NAK
                 buffer[ANSW_SOH_IDX] = SOH;
                 buffer[ANSW_ACK_IDX] = NAK;
                 sendBuffer(buffer, ANSW_PACKET_SIZE);

                 while (getBuffer(buffer, 1, ACK_TIMEOUT) == 0)
                     ;
                 return -1;
         }
     } while (1);
}



