#include "caff_i2c_grovelcdrgb.h"

CAff_i2c_GroveLcdRgb::CAff_i2c_GroveLcdRgb()
{
    m_i2c = CI2c::getInstance(this, '1'); // new objet de la classe Ci2c
    connect(m_i2c, SIGNAL(sigErreur(QString)), this, SLOT(onErreur(QString)));
    clear();
    setColorOff();
}

CAff_i2c_GroveLcdRgb::~CAff_i2c_GroveLcdRgb()
{
  clear();
  setColorOff();
  CI2c::freeInstance();
  qDebug() << "Objet CAff_i2c_GroveLcdRgb détruit !";
}

void CAff_i2c_GroveLcdRgb::begin(int lines, int dotsize)
{

    if (lines > 1) {
        m_displayfunction |= LCD_2LINE;
    }
    m_numlines = lines;
    m_currline = 0;
    // for some 1 line displays you can select a 10 pixel high font
    if ((dotsize != 0) && (lines == 1)) {
        m_displayfunction |= LCD_5x10DOTS;
    }
    // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    // according to datasheet, we need at least 40ms after power rises above 2.7V
    // before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
    QThread::usleep(50000);
    // this is according to the hitachi HD44780 datasheet
    // page 45 figure 23
    // Send function set command sequence
    command(LCD_FUNCTIONSET | m_displayfunction);
    QThread::usleep(4500);  // wait more than 4.1ms
    // second try
    command(LCD_FUNCTIONSET | m_displayfunction);
    QThread::usleep(150);
    // third go
    command(LCD_FUNCTIONSET | m_displayfunction);
    // finally, set # lines, font size, etc.
    command(LCD_FUNCTIONSET | m_displayfunction);
    // turn the display on with no cursor or blinking default
    m_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    display();
    // clear it off
    clear();
    // Initialize to default text direction (for romance languages)
    m_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    // set the entry mode
    command(LCD_ENTRYMODESET | m_displaymode);
    // backlight init
    setReg(REG_MODE1, 0);
    // set LEDs controllable by both PWM and GRPPWM registers
    setReg(REG_OUTPUT, 0xFF);
    // set MODE2 values
    // 0010 0000 -> 0x20  (DMBLNK to 1, ie blinky mode)
    setReg(REG_MODE2, 0x20);
    setColorWhite();
}

/********** high level commands, for the user! */
void CAff_i2c_GroveLcdRgb::clear()
{
    command(LCD_CLEARDISPLAY);        // clear display, set cursor position to zero
    QThread::usleep(2000);          // this command takes a long time!
}

void CAff_i2c_GroveLcdRgb::home()
{
    command(LCD_RETURNHOME);        // set cursor position to zero
    QThread::usleep(2000);        // this command takes a long time!
}

void CAff_i2c_GroveLcdRgb::setCursor(int col, int row)
{
    col = (row==0?col|0x80:col|0xc0);
    unsigned char data[2] = {0x80,(unsigned char)col};
    m_i2c->ecrire(LCD_ADDRESS, data, 2);
    QThread::msleep(200);
}

// Turn the display on/off (quickly)
void CAff_i2c_GroveLcdRgb::noDisplay()
{
    m_displaycontrol &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | m_displaycontrol);
}

void CAff_i2c_GroveLcdRgb::display() {
    m_displaycontrol |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | m_displaycontrol);
}

// Turns the underline cursor on/off
void CAff_i2c_GroveLcdRgb::noCursor()
{
    m_displaycontrol &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | m_displaycontrol);
}

void CAff_i2c_GroveLcdRgb::cursor() {
    m_displaycontrol |= LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | m_displaycontrol);
}

// Turn on and off the blinking cursor
void CAff_i2c_GroveLcdRgb::noBlink()
{
    m_displaycontrol &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | m_displaycontrol);
}
void CAff_i2c_GroveLcdRgb::blink()
{
    m_displaycontrol |= LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | m_displaycontrol);
}

// These commands scroll the display without changing the RAM
void CAff_i2c_GroveLcdRgb::scrollDisplayLeft(void)
{
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void CAff_i2c_GroveLcdRgb::scrollDisplayRight(void)
{
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void CAff_i2c_GroveLcdRgb::leftToRight(void)
{
    m_displaymode |= LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | m_displaymode);
}

// This is for text that flows Right to Left
void CAff_i2c_GroveLcdRgb::rightToLeft(void)
{
    m_displaymode &= ~LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | m_displaymode);
}

// This will 'right justify' text from the cursor
void CAff_i2c_GroveLcdRgb::autoscroll(void)
{
    m_displaymode |= LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | m_displaymode);
}

// This will 'left justify' text from the cursor
void CAff_i2c_GroveLcdRgb::noAutoscroll(void)
{
    m_displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | m_displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void CAff_i2c_GroveLcdRgb::createChar(int location, int charmap[])
{
    location &= 0x7; // we only have 8 locations 0-7
    command(LCD_SETCGRAMADDR | (location << 3));
    unsigned char data[9];
    data[0] = 0x40;
    for(int i=0; i<8; i++)
        data[i+1] = charmap[i];
    m_i2c->ecrire(LCD_ADDRESS, data, 9);
    QThread::msleep(200);
}

// Control the backlight LED blinking
void CAff_i2c_GroveLcdRgb::blinkLED(void)
{
    // blink period in seconds = (<reg 7> + 1) / 24
    // on/off ratio = <reg 6> / 256
    setReg(0x07, 0x17);  // blink every second
    setReg(0x06, 0x7f);  // half on, half off
}

void CAff_i2c_GroveLcdRgb::noBlinkLED(void)
{
    setReg(0x07, 0x00);
    setReg(0x06, 0xff);
}

/*********** mid level commands, for sending data/cmds */

// send command
inline void CAff_i2c_GroveLcdRgb::command(int value)
{
    unsigned char data[2] = {0x80, (unsigned char)value};
    m_i2c->ecrire(LCD_ADDRESS, data, 2);
    QThread::msleep(200);
}

void CAff_i2c_GroveLcdRgb::afficherMesures(float tempTc72, float tempSht20, float humSht20, bool seuil)
{
    static int noC=2;
    if (noC==2) noC=3; else noC=2;
    if (seuil) noC=1;
    setColor(noC);
    setCursor(0,0);
    ecrire("I2C: "+QString::number(tempSht20,'f',0)+
           "dC "+QString::number(humSht20,'f',0)+"%");
    setCursor(0,1);
    ecrire("SPI: "+QString::number(tempTc72,'f',0)+"dC");
}

void CAff_i2c_GroveLcdRgb::sequenceBienvenue()
{
    begin(16,2);
    setRGB(0,0,255);
    setCursor(5, 0);
    ecrire("Bonjour");
    setCursor(4, 1);
    ecrire("Apprenant");
    QThread::sleep(2);
    clear();
    setColor(WHITE);
    setCursor(0, 0);
    ecrire("QT/C++...");
    setCursor(0, 1);
    ecrire("YES !!!");
    QThread::usleep(250000);
    for(int i=0 ; i<10 ; i++)
       scrollDisplayRight();
    begin(16,2);
    setRGB(0,0,0);
    emit workFinished();
}

// send data
int CAff_i2c_GroveLcdRgb::ecrire(QString text)
{
    for(int i=0;i<text.length();i++)
    {
       unsigned char data[2] = {0x40, text.at(i).toLatin1()};
       m_i2c->ecrire(LCD_ADDRESS, data, 2);
       QThread::msleep(10);
    }
    return 1; // assume sucess
}

void CAff_i2c_GroveLcdRgb::setReg(unsigned char adr, unsigned char data)
{
    unsigned char buffer[2];
    buffer[0] = adr;
    buffer[1] = data;
    m_i2c->ecrire(RGB_ADDRESS, buffer, 2);
    QThread::usleep(200);
}

void CAff_i2c_GroveLcdRgb::onErreur(QString mess)
{
    emit sigErreur(mess);
}

void CAff_i2c_GroveLcdRgb::setRGB(unsigned char r, unsigned char g, unsigned char b)
{
    setReg(REG_RED, r);
    setReg(REG_GREEN, g);
    setReg(REG_BLUE, b);
}

const unsigned char color_define[][3] =
{
    {255, 255, 255},            // white
    {255, 0, 0},                // red
    {0, 255, 0},                // green
    {0, 0, 255},                // blue
    {0, 0, 0}                   // black
};

void CAff_i2c_GroveLcdRgb::setColor(unsigned char color)
{
    if(color > 4)return ;
    setRGB(color_define[color][0], color_define[color][1], color_define[color][2]);
}
