/*
这是程永康写的“中位机”的程序。顾名思义中位机就是介于上位机和下位机之间的机器
电脑给中位机发送指令，中位机将指令通过红外线转达给光电门，并将光电门返回的信息原封不动地转达给电脑
*/

#define iremt 7       // infrared emit 红外发射引脚
#define irrcv 3       // infrared receive
#define lsemt 6       // laser emit
#define lsrcv 2       // laser receive
#define ir_maxlen 32  // 红外线通讯时接收信号的最大长度
#define id 0

unsigned long time_stamp;    // 这个变量保存着每个函数开始/结束的时间，或者是一些重要的时间戳
char _send_buffer[ir_maxlen] = {0};    // 这是发送端的缓冲区，是send函数直接发送的内容
char _recv_buffer[ir_maxlen] = {0};    // 这是接收端的缓冲区，是receive函数存储接收到的数据的地方
char send_buffer[ir_maxlen-2] = {0};  // 这是存储准备发送的信息的缓冲区，是人能直接读懂的内容。调用encode函数后会存到发送端缓冲区
char recv_buffer[ir_maxlen-2] = {0};  // 这是存储接收到的消息的缓冲区，存储由decode函数分离出来的消息
unsigned long for_adj_time = 0;

// ###############################################################################
// 接下来是有关红外发射的函数们
void send_BOF() {
  // 发送 beginning of file
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
  // 发送一个bit的信息
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
  // 发送ending of file
  // 发送1000微秒的高频脉冲
  time_stamp += 1000;
  while (micros() < time_stamp) {
    digitalWrite(iremt, HIGH);
    delayMicroseconds(5);
    digitalWrite(iremt, LOW);
    delayMicroseconds(15);
  }
}

size_t encode() {
  // 接下来是给字符串编码的函数
  // 将send_m_buff里的内容加入校验位并转存到send_buffer里
  strcpy(_send_buffer, send_buffer);
  // 开始计算校验位，使用XOR校验
  char xor_checker = 0;
  size_t s_buf_len = strlen(send_buffer) + 1;    // 加了一个'\0'
  for (size_t i=0; i<s_buf_len; i++) {
    xor_checker ^= _send_buffer[i];
  }
  _send_buffer[s_buf_len] = xor_checker;
  return ++s_buf_len;                               // 再加一个校验位
}

void send() {
  // 发送红外信息的函数，里面自动发送开始位、消息内容，和结束位
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

// #########################################################################
// 接下来是有关红外接收的函数们
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
  // 接收一个bit。返回内容：0=0, 1=1, 2=EOF,-1=ERROR
  // 有250μs的时间容忍度
  static unsigned long duration;
  static unsigned long timeout;
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

int decode(int r_buf_length) {
  // 将recv_buffer里的内容进行校对，将有用的信息转到recv_m_buff里
  // 成功返回0，发现id不对或者校验失败，就返回-1
  // 先进行xor校验
  // 因为消息的最后一位是校验位，所以校验位最后会xor自己，
  // 所以在接收正确的情况下xor checker应该等于零
  char xor_checker = 0;
  for (int i=0; i<r_buf_length; i++) {
    xor_checker ^= _recv_buffer[i];
  }
  if (xor_checker)
    return -1;
  // 检查消息发送的id是否是自己的id
  // 换言之检查这个消息是不是给自己发送的
  char message_id = _recv_buffer[0];
  if (message_id != id)
    return -1;
  // 将消息转存到message buffer里
  memset(recv_buffer, 0, ir_maxlen-2);
  strcpy(recv_buffer, _recv_buffer+1);
  return 0;
}

int receive() {
  // 接收raw data，自动decode，成功返回0，不成功返回-1
  // 将data存到recv_buffer里
  // 这里的前提条件是刚刚检测到了红外信号的开端
  static size_t buf_len;
  static int bit_pos;
  static char c;
  static int b;
  memset(_recv_buffer, 0, ir_maxlen);   // 将buffer清空
  for (buf_len=0; buf_len<ir_maxlen; buf_len++) {
    c = 0;
    for (bit_pos=7; bit_pos>=0; bit_pos--) {
      b = receive_bit();
      if (b == -1)    // 如果这个子函数报错了，那就报错
        return -1;
      if (b == 2)     // 如果收到了结束信号（EOF），就返回
        return decode(buf_len);
      // 走到这说明正常收到了一个bit
      // 将收到的bit存到字符串中
      c |= b << bit_pos;
    }
    _recv_buffer[buf_len] = c;
  }
  return -1;   // 这里超出buffer的范围了也是要报错的
}

// #########################################################################
// 接下来是与电脑交互的函数和杂七杂八的函数们……
size_t input_available() {
  // 检测电脑端是否有消息。如果有的话就等待一段时间收集，然后返回消息的长度
  static size_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == ';') {   // 遇到结束标识符（分号）后返回
      recv_buffer[len] = '\0';
      size_t temp = len;
      len = 0;
      return temp;
    }
    recv_buffer[len] = c;
    len++;
  }
  return 0;
}

void _print(char* command) {
// 开发使用的函数，打印出字符串里的0和1
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

/*bool is_numeric(char *str) {
  if (!*str)
    return false;
  if (!isdigit(*str) & *str != '-')
    return false;
  str++;
  while (*str) {
    if (!isdigit(*str))
      return false;
    str++;
  }
  str--;
  return isdigit(*str);
}*/

char lld_buf[20];
void lld_to_str(char* str, long long num) {
  // if it is negative
  bool negative = false;
  if (num < 0) {
    negative = true;
    num = -num;
  }
  memset(lld_buf, 0, 20);
  int idx = 0;
  char c;
  while (num) {
    c = 48 + num % 10;
    num /= 10;
    lld_buf[idx] = c;
    idx++;
  }
  if (negative)
    lld_buf[idx++] = '-';
  // reverse the sequence
  int i = 0;
  while (idx >= 0)
    str[i++] = lld_buf[--idx];
  str[i] = '\0';
}

long long my_time, p_time, adj_time;
void adjust_time() {
  strcpy(send_buffer, recv_buffer);
  send();
  unsigned long timeout = millis() + 500;
  while (!check_ir()) {
    if (millis() > timeout) {         // 超时了就不等了，回复timeout
      strcpy(send_buffer, "timeout");
      return;
    }
  }
  for_adj_time = micros();
  // 接收消息
  if (receive()) {
    strcpy(send_buffer, "receive error");
    return;
  }
  // convert the number
  my_time = (long long)for_adj_time;
  char* endPtr;
  p_time = (long long)strtoul(recv_buffer, &endPtr, 10);
  // if convert failed
  if (p_time == 0) {
    strcpy(send_buffer, "convert failed");
    return;
  }
  adj_time = my_time - p_time;
  lld_to_str(send_buffer, adj_time);
  return;
}

void process_command() {
  // 处理recv_m_buff中的指示，回复的消息存在send_m_buff中
  if (!strcmp(recv_buffer, "Are you ok")) { // 如果这样问的话直接回复Yes就行了
    strcpy(send_buffer, "Yes");
    delay(10);
    return;
  }
  // if it is "ADJT" which means adjust time, then adjust time it self
  // for the accurate time
  if (!strcmp(recv_buffer+2, "ADJT")) {
    adjust_time();
    return;
  }
  // 走到这的话，直接将消息原封不动地转发给光电门就行了
  strcpy(send_buffer, recv_buffer);
  send();
  //_print(_send_buffer);
  // 等待回复
  unsigned long timeout = millis() + 300;
  while (!check_ir()) {
    if (millis() > timeout) {         // 超时了就不等了，回复timeout
      strcpy(send_buffer, "timeout");
      return;
    }
  }
  // 接收消息
  if (receive()) {
    strcpy(send_buffer, "receive error");
    return;
  }
  strcpy(send_buffer, recv_buffer);
  return;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(iremt, OUTPUT);
  pinMode(irrcv, INPUT);
  Serial.begin(9600);
}

void loop() {
  // 直到接收到电脑端消息才进行下一步
  if (!input_available())
    return;
  // 处理消息，处理完成后send_m_buffer应该就准备好了
  process_command();
  // 发送消息给电脑
  Serial.println(send_buffer);
}
