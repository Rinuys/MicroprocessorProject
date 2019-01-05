/*
 * AirCleaner.c
 *
 * Created: 2018-11-20 화 오후 9:33:51
 * Author : Rinuys
 */ 

#include<avr/io.h>
#include<avr/interrupt.h>      //인터럽트 헤더 파일
#define	F_CPU 16000000UL
#include<util/delay.h>        //딜레이 헤더 파일

#define LCD_WDATA PORTA     //LCD데이터 포트 정의
#define LCD_WINST PORTA    //
#define LCD_CTRL PORTC    //LCD 제어 포트 정의
#define LCD_EN 0         //LCD 제어(PING0~2)를 효과적으로 하기위한 정의
#define LCD_RW 1
#define LCD_RS 2
#define Byte unsigned char

void delay_us(int n){
	for(int i = 0 ; i < n ; i++);
}

void delay_ms(int n){
	for(volatile int i = 0 ; i < n ; i++)
	for(int j = 0 ; j < 1000 ; j++);
}

void LCD_Data(Byte ch){
	delay_ms(2);
	LCD_WDATA = ch;                //데이터 출력
	LCD_CTRL |=  (1 << LCD_RS);    //RS=1, R/W=0 으로 데이터 쓰기 싸이클
	LCD_CTRL &= ~(1 << LCD_RW);
	LCD_CTRL |=  (1 << LCD_EN);    //LCD 사용
	LCD_CTRL &= ~(1 << LCD_EN);    //LCD 사용안함
}

								   //  0         1      2
void LCD_Comm(Byte ch){			   //PG0-EN , PG1-RW, PG2-RS , PG4-TOSC1핀(사용안함)
							       //LCD_CTRL = LCD제어부 포트(4핀인데 실질적인 사용3핀)
	delay_ms(2);
	LCD_WINST = ch;                //명령어 쓰기
	LCD_CTRL &= ~(1 << LCD_RS);    // RS=0, RW=0 으로 정의함.
	LCD_CTRL &= ~(1 << LCD_RW);
	LCD_CTRL |=  (1 << LCD_EN);    //LCD 사용허가
	LCD_CTRL &= ~(1 << LCD_EN);    //LCD 사용안함
}

void LCD_CHAR(Byte c){  //한문자 출력 함수
	LCD_Data(c);		//CGROM 문자코드의 0x31 ~ 0xFF 는 아스키코드와 일치함!
	delay_ms(2);
}

void LCD_STR(Byte *str){    //↑문자열을 한문자씩 출력함수로 전달
	while(*str !=0){
		LCD_CHAR(*str);
		str++;
	}
}

void LCD_pos(Byte col, Byte row){  //LCD 포지션 설정
	LCD_Comm(0x80 | (row+col*0x40));  // row=행 / col=열 ,DDRAM주소설정
}

void LCD_Clear(void){  // 화면 클리어
	LCD_Comm(0x01);
	delay_ms(2); //1.6ms이상의 실행시간소요로 딜레이필요
}

void LCD_Init(void){   //LCD 초기화
	DDRA = 0xff; //PORTA(LCD연결포트)를 출력으로 설정
	DDRC = 0xff; //PORTG(LCD컨트롤)의 하위 4비트를 출력으로설정

	LCD_Comm(0x38);   //데이터 8비트 사용, 5X7도트 , LCD2열로 사용(6)
	delay_ms(2);
	LCD_Comm(0x0f);   //Display ON/OFF
	delay_ms(2);
	LCD_Comm(0x06);   //주소 +1 , 커서를 우측으로 이동 (3)
	delay_ms(2);
	LCD_Clear();
}

volatile long pcs = 0; // 측정된 시간의 합산
volatile Byte str1[30] = ""; // 출력할 문자열
volatile unsigned int cont = 0; // 정수부
volatile unsigned int flt = 0; // 소수부
const int time_ms = 30000; // 30초간 측정
volatile unsigned int pulse_state=0; // 0 : Up
									 // 1 : Down


volatile long particle_counter=0;
ISR(INT6_vect){
	if(pulse_state == 0){ // Down Event
		pulse_state = 1;
		particle_counter = 0; // 1당 100us단위
	}
	else{ // Up Event
		pulse_state = 0;
		pcs += particle_counter*10; // ms단위의 시간을 합산
	}
}

ISR(TIMER0_OVF_vect){ // 한 사이클당 100us=0.1ms
	if(pulse_state == 1) // 측정 상태이면 counter를 증가
		particle_counter++;
	TCNT0 = 206;
}

volatile long motor_count=0;
ISR(TIMER2_COMP_vect){
	motor_count++;
}

void motor(int sec, int speed_val){ // 모터동작함수
	OCR2 = speed_val; // 속도 조절 
	TCNT2 = 0;
	while(motor_count != 1000*sec); // 30초 동작
	motor_count = 0;
}

int main(){
	DDRE = 0xbf; // E6번 Input
	EIMSK = 0x40;
	EICRB = 0x30;// 논리적 변화시 인터럽트
	TCCR0 = 0x03;
	TIMSK = 0x81;
	TCNT0 = 206; // 100us
	TCCR2 = 0x7B;
	DDRB = 0xff;
	sei();
	
	int motor_time=time_ms/1000;
	LCD_Init(); //LCD 초기화
	delay_ms(20);
	LCD_pos(0,0); //LCD 포지션 0행 1열 지정
	delay_ms(20);
	sprintf(str1,"Initialize...");
	LCD_STR(str1); //문자열 str을 LCD출력
	delay_ms(20);
	
	while(1){
		delay_ms(time_ms);
		float ratio = pcs*1.0/(time_ms*10.0);
		pcs=0;
		float concentration = (1.1*ratio*ratio*ratio
		-3.8*ratio*ratio+520.0*ratio+0.62)/130.0;
		cont = (unsigned int)concentration;
		flt = (unsigned int)((concentration-cont)*1000.0);
		sprintf(str1,"%d.%d ug/m3",cont,flt);
		
		LCD_Init(); //LCD 초기화
		delay_ms(20);
		LCD_pos(0,0); //LCD 포지션 0행 1열 지정
		delay_ms(20);
		LCD_STR(str1); //문자열 str을 LCD출력
		delay_ms(20);
		LCD_pos(1,0); //LCD 포지션 0행 1열 지정
		delay_ms(20);
		
		unsigned char str2[10];
		if(cont<16){
			sprintf(str2,"GOOD");
			LCD_STR(str2); //문자열 str을 LCD출력
			delay_ms(20);
			motor(motor_time,255);
		}
		else if(cont<36){
			sprintf(str2,"NORMAL");
			LCD_STR(str2); //문자열 str을 LCD출력
			delay_ms(20);
			motor(motor_time,100);
		}
		else if(cont<76){
			sprintf(str2,"BAD");
			LCD_STR(str2); //문자열 str을 LCD출력
			delay_ms(20);
			motor(motor_time,50);
		}
		else{
			sprintf(str2,"VERY BAD");
			LCD_STR(str2); //문자열 str을 LCD출력
			delay_ms(20);
			motor(motor_time,0);
		}
	}
}
