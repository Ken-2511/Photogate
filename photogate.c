/*
这是程永康写的放在光电门上的程序，写得可能有点草率/不标准，请多多见谅
作为一个C语言小白，程永康边学边写，问了ChatGPT好多问题……
我会尽量多写注释的，希望你能看懂。
*/

#include <limits.h>
#include <TM1637Display.h>

#define iremt 7       // infrared emit 红外发射引脚
#define irrcv 3       // infrared receive
#define lsemt 6       // laser emit
#define lsrcv 2       // laser receive
#define ir_maxlen 32  // 红外线通讯时接收信号的最大长度
#define nixie_clk 5   // 数码管的时钟线
#define nixie_dio 4   // 数码管的数据线
#define battery 0

unsigned long time_stamp;    // 这个变量保存着每个函数开始/结束的时间，或者是一些重要的时间戳
char receive_buffer[ir_maxlen] = {0};
char send_buffer[ir_maxlen] = {0};      // 这是发送端的缓冲区
char message_buffer[ir_maxlen-2] = {0};
char _send_buffer[ir_maxlen] = {0};
char id = '0';

TM1637Display display = TM1637Display(nixie_clk, nixie_dio);

// ########################################################
// 以下是红外线发送的相关函数

void send_BOF() {
  time_stamp = micros();
  time_stamp += 5000;
  while (micros() < time_stamp) {
    digitalWrite(iremt, HIGH);
    delayMicroseconds(5);
    digitalWrite(iremt, LOW);
    delayMicroseconds(15);
  }
  time_stamp += 5000;
  while (micros() < time_stamp);
}

void send_bit(bool data) {
  // 先发送500微秒高频信号
  time_stamp += 500;
  while (micros() < time_stamp) {
    digitalWrite(iremt, HIGH);
    delayMicroseconds(5);
    digitalWrite(iremt, LOW);
    delayMicroseconds(15);
  }
  // 静默一定时间：0 = 500, 1 = 1000
  if (data)
    time_stamp += 1000;
  else
    time_stamp += 500;
  while (micros() < time_stamp);
}

void send_EOF() {
  // 发送1000毫秒的高频脉冲
  time_stamp += 1000;
  while (micros() < time_stamp) {
    digitalWrite(iremt, HIGH);
    delayMicroseconds(5);
    digitalWrite(iremt, LOW);
    delayMicroseconds(15);
  }
}

void send() {
  static size_t i;
  static int bit_pos;
  static char c;
  static bool b;
  // 先encode
  size_t length = encode();
  // 先发送开始位
  send_BOF();
  // 再发送消息内容
  for (i=0; i<length; i++) {
    c = _send_buffer[i];
    for (bit_pos=7; bit_pos>=0; bit_pos--) {
      b = (c >> bit_pos) & 1;
      send_bit(b);
    }
  }
  // 再发送结束位
  send_EOF();
}

// ########################################################
// 以下是红外线接收的相关函数

bool check_ir() {
  // 检测是否看见了红外信号
  static unsigned long last_high = 0;   // 上次检测到高电平的时间
  static unsigned long now;
  static unsigned long timeout;
  now = micros();
  // 现在如果是高电平，肯定不符合条件的，就直接返回false
  if (digitalRead(irrcv)) {
    last_high = now;
    return false;
  }
  // 检查现在是否在10ms区间内，只有在10ms区间内才符合要求。（有1ms模糊判定）
  if ((now - last_high < 4000) | (now - last_high > 6000)) {
    return false;
  }
  // 以下是在10ms区间内的情况
  timeout = now + 2000;
  while (!digitalRead(irrcv)) {   // 等待变成高电平
    // 如果超时就返回false
    if (micros() > timeout)
      return false;
  }
  // 以下是在5ms区间内的情况
  timeout = micros() + 6000;
  while (digitalRead(irrcv)) {
    // 如果超时就返回false
    if (micros() > timeout)
      return false;
  }
  time_stamp = micros();
  return true;
}

int receive_bit() {
  static unsigned long duration;
  static unsigned long timeout;
  // 接收一个bit。返回内容：0=0, 1=1, 2=EOF,-1=ERROR
  // 有250μs的时间容忍度
  // 当前是低电平，等待500μs/1000μs的小波过去
  timeout = time_stamp + 1250;
  while (!digitalRead(irrcv)) {
    if (micros() > timeout)
      return -1;
  }
  // 如果脉冲时间是1000μs，说明是EOF
  duration = micros() - time_stamp;
  if (duration > 750)
    return 2;
  if (duration < 250)
    return -1;
  // 当前是高电平，通过高电平的时间判断发送的是何种数据
  // 500μs=0, 1000μs=1
  time_stamp = micros();
  timeout = time_stamp + 1250;
  while (digitalRead(irrcv)) {
    if (micros() > timeout)
      return -1;
  }
  // 开始判断接收到的是何种数据
  duration = micros() - time_stamp;
  time_stamp = micros();
  if (duration > 750)
    return 1;
  if (duration > 250)
    return 0;
  return -1;
}

int receive() {
  // 接收raw data，成功返回字符数，不成功返回0
  // 这里的前提条件是刚刚检测到了红外信号的开端
  static size_t buf_len;
  static int bit_pos;
  static char c;
  static int b;
  memset(receive_buffer, 0, ir_maxlen);   // 将buffer清空
  for (buf_len=0; buf_len<ir_maxlen; buf_len++) {
    c = 0;
    for (bit_pos=7; bit_pos>=0; bit_pos--) {
      b = receive_bit();
      if (b == -1)    // 如果这个子函数报错了，那就报错
        return 0;
      if (b == 2)     // 如果收到了结束信号（EOF），就返回
        return buf_len;
      // 走到这说明正常收到了一个bit
      // 将收到的bit存到字符串中
      c |= b << bit_pos;
    }
    receive_buffer[buf_len] = c;
  }
  return 0;   // 这里超出buffer的范围了也是要报错的
}

/*以下是各种各样的命令，这些命令是红外线/串口调用的
每当处理器接收到一个command，主程序会循环调用这些函数，并传入参数`command`
`command`是一个字符串。如果command里的第一个单词与函数里的密令是一样的，
就说明command就是想要调用这个函数。*/

int control_laser(char* command)
// 命令：控制激光灯的亮灭
{
  if (!strcmp(command, "ON"))
  {
    digitalWrite(lsemt, HIGH);
    return 0;
  }
  if (!strcmp(command, "OFF"))
  {
    digitalWrite(lsemt, LOW);
    return 0;
  }
  return -1;
}

unsigned long laser_seen_time = 0;;
bool laser_seen = false;
void laser_interrupt()
// 激光触发的中断程序，只能触发一次
{
  if (laser_seen)
    return;
  laser_seen_time = micros();
  laser_seen = true;
  digitalWrite(lsemt, LOW);
  uint8_t segment[4] = {0x6d, 0x79, 0x79, 0x37};
  display.setSegments(segment);
}

int monitor(char* command)
// 命令：开始/停止监测激光
{
  if (!strcmp(command, "ON"))
  {
    digitalWrite(lsemt, HIGH);
    uint8_t segment[4] = {64, 64, 64, 64};
    display.setSegments(segment);
    delay(50);
    detachInterrupt(digitalPinToInterrupt(lsrcv));
    attachInterrupt(digitalPinToInterrupt(lsrcv), laser_interrupt, RISING);
    laser_seen = false;
    return 0;
  }
  if (!strcmp(command, "OFF"))
  {
    detachInterrupt(digitalPinToInterrupt(lsrcv));
    digitalWrite(lsemt, LOW);
    display.clear();
    return 0;
  }
  return -1;
}

void laser_debug_interrupt() {
  static uint8_t yes[4] = {0, 110, 121, 109};
  static uint8_t no[4] = {0, 0, 84, 92};
  if (digitalRead(lsrcv))   // means can not see
    display.setSegments(no);
  else
    display.setSegments(yes);
}

int debug_laser(char* command)
{
  if (!strcmp(command, "ON")) {
    digitalWrite(lsemt, HIGH);
    detachInterrupt(digitalPinToInterrupt(lsrcv));
    attachInterrupt(digitalPinToInterrupt(lsrcv), laser_debug_interrupt, CHANGE);
    laser_debug_interrupt();
    return 0;
  }
  if (!strcmp(command, "OFF")) {
    digitalWrite(lsemt, LOW);
    detachInterrupt(digitalPinToInterrupt(lsrcv));
    display.clear();
    return 0;
  }
  return -1;
}

int check_laser()
// 命令：检查激光是否检测到有物品经过
{
  if (laser_seen)
  {
    strcpy(send_buffer, "true");
  } else {
    strcpy(send_buffer, "false");
  }
  return 0;
}

int tell_time()
// 命令：汇报激光检测到物品的时间
{
  sprintf(send_buffer, "%lu", laser_seen_time);
  return 0;
}

int adjust_time()
{
  sprintf(send_buffer, "%lu", micros());
  return 0;
}

int tell_voltage()
{
  sprintf(send_buffer, "%d", analogRead(battery));
  return 0;
}

char nixie_segment[4];
int set_segment(char* command)
{
  uint8_t segment[4];
  for (int i=0; i<4; i++) {
    if (command[i] == ' ')
      command[i] = '\0';
    segment[i] = (uint8_t)(command[i]);
  }
  display.setSegments(segment);
  return 0;
}

int set_brightness(char* command)
{
  if (strlen(command) != 1)
    return -1;
  char* endptr;
  unsigned long result;
  result = strtoul(command, &endptr, 10);
  if (result > 7)
    return -1;
  display.setBrightness((uint8_t)result);
  return 0;
}

int set_number_colon(char* command) {
  if (strlen(command) != 4)
    return -1;
  for (int i=0; i<4; i++) {
    if (!isdigit(command[i]))
      return -1;
  }
  char *endptr;
  int num = (int)strtoul(command, &endptr, 10);
  Serial.println(num);
  display.showNumberDecEx(num, 64);
  return 0;
}

int are_you_ok(char* command) {
  if (strlen(command) != 0)
    return -1;
  return 0;
}

void _print(char* command)
// 开发使用的函数，打印出字符串里的0和1
{
  Serial.print("print...[");
  Serial.print(command);
  Serial.print("]: ");
  for (int i=0; i<ir_maxlen; i++)
    {
      Serial.print(command[i]);
      for (int j=7; j>=0; j--)
      {
        Serial.print(command[i] >> j & 1);
      }
      Serial.print(" ");
    }
    Serial.println();
}

/*给定字符串，根据字符串调用合适的函数*/
void process_command(char* command)
{
  // 准备send_buffer，默认是`ok`
  memset(send_buffer, 0, ir_maxlen);
  strcpy(send_buffer, "ok");
  // token是第一个单词，rest是剩下的部分
  char temp[ir_maxlen];
  strcpy(temp, command);
  char *token, *rest = temp;
  token = strsep(&rest, " ");
  int result;
  if (!strcmp(token, "LSR")) {            // laser
    result = control_laser(rest);
  } else if (!strcmp(token, "MNT")) {     // monitor
    result = monitor(rest);
  } else if (!strcmp(token, "CHKL")) {    // check laser
    result = check_laser();
  } else if (!strcmp(token, "TLTM")) {    // tell time
    result = tell_time();
  } else if (!strcmp(token, "ADJT")) {    // adjust time
    result = adjust_time();
  } else if (!strcmp(token, "NXB")) {     // nixie brightness
    result = set_brightness(rest);
  } else if (!strcmp(token, "NXS")) {     // nixie set segment
    result = set_segment(rest);
  } else if (!strcmp(token, "NXN")) {     // nixie set number
    result = set_number_colon(rest);
  } else if (!strcmp(token, "TLV")) {     // tell voltage
    result = tell_voltage();
  } else if (!strcmp(token, "DBL")) {     // debug laser
    result = debug_laser(rest);
  } else if (!strcmp(token, "RUOK")) {    // check whether alive
    result = are_you_ok(rest);
  } else {
    result = -1;
  }
  if (result == -1) {
    strcpy(send_buffer, "BAD CMD");
  }
}

int decode(int r_buf_length) {
  // 将receive buffer里的内容进行校对，将有用的信息转到message buffer里
  // 成功返回0，发现id不对或者校验失败，就返回-1
  // 先进行xor校验
  // 因为消息的最后一位是校验位，所以校验位最后会xor自己，
  // 所以在接收正确的情况下xor checker应该等于零
  char xor_checker = 0;
  for (int i=0; i<r_buf_length; i++) {
    xor_checker ^= receive_buffer[i];
  }
  if (xor_checker)
    return -1;
  // 检查消息发送的id是否是自己的id
  // 换言之检查这个消息是不是给自己发送的
  char message_id = receive_buffer[0];
  if (message_id != id)
    return -1;
  // 将消息转存到message buffer里
  memset(message_buffer, 0, ir_maxlen-2);
  strcpy(message_buffer, receive_buffer+2);
  return 0;
}

size_t encode() {
  // 接下来是给字符串编码的函数
  // 将send_m_buff里的内容加入校验位并转存到send_buffer里
  _send_buffer[0] = 0;    // 因为主控的id是0，光电门肯定是给主控发消息
  strcpy(_send_buffer+1, send_buffer);
  // 开始计算校验位，使用XOR校验
  char xor_checker = 0;
  size_t s_buf_len = strlen(send_buffer) + 2;    // 加了一个'\0'
  for (size_t i=0; i<s_buf_len; i++) {
    xor_checker ^= _send_buffer[i];
  }
  _send_buffer[s_buf_len] = xor_checker;
  return ++s_buf_len;                               // 再加一个校验位
}

void setup() {
  pinMode(iremt, OUTPUT);
  pinMode(irrcv, INPUT);
  pinMode(lsemt, OUTPUT);
  Serial.begin(9600);
  display.setBrightness(7);
  display.showNumberDec(atoi(&id));
}

void loop() {
  // 等待接收
  if (!check_ir())
    return;
  int r_num = receive();
  if (!r_num)
    return;
  if (decode(r_num))
    return;
  process_command(message_buffer);
  // 等待一定时间回复
  delay(10);
  send();
  Serial.println(send_buffer);
}
