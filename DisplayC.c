#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/adc.h"     
#include "hardware/pwm.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define GREEN 11
#define BLUE 12
#define RED 13
#define BUTTON_A 5
#define BUTTON_B 6
#define VRX_PIN 26   
#define VRY_PIN 27   
#define JOYSTICK_BUTTON 22

uint32_t last_time=0,last_time2=0;
ssd1306_t ssd; // Inicializa a estrutura do display
bool leds_ativos=1,cor=1;
uint8_t teste1,teste2;

uint pwm_init_gpio(uint gpio, uint wrap) {
  gpio_set_function(gpio, GPIO_FUNC_PWM);

  uint slice_num = pwm_gpio_to_slice_num(gpio);
  pwm_set_wrap(slice_num, wrap);
  
  pwm_set_enabled(slice_num, true);  
  return slice_num;  
}

//Rotina de interrupção
void interrupt(uint gpio, uint32_t events)
{
    // Obtem o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time2 > 300000) // 300 ms de debouncing
    {
      last_time2=current_time;
      if(gpio == JOYSTICK_BUTTON){
        cor = !cor;
        ssd1306_fill(&ssd, !cor);
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
        ssd1306_draw_square(&ssd,teste2,teste1);
        ssd1306_send_data(&ssd);
        gpio_put(GREEN,!gpio_get(GREEN));
      }
      else{
       leds_ativos= (!leds_ativos);
       pwm_set_gpio_level(RED,0);
       pwm_set_gpio_level(BLUE,0);
       printf("leds_ativos = %d\n",leds_ativos);
      }
    }
}

int main()
{
  uint16_t red_level,blue_level;
  uint32_t current_time;
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA); // Pull up the data line
  gpio_pull_up(I2C_SCL); // Pull up the clock line
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display

  adc_init();
  adc_gpio_init(VRX_PIN); 
  adc_gpio_init(VRY_PIN); 
  gpio_init(JOYSTICK_BUTTON);
  gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
  gpio_pull_up(JOYSTICK_BUTTON); 

    stdio_init_all();

    gpio_init(GREEN);              
    gpio_set_dir(GREEN, GPIO_OUT);
    gpio_put(GREEN,0);
    
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BUTTON_A);          // Habilita o pull-up interno

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BUTTON_B);

    pwm_init_gpio(RED, 4096);
    pwm_init_gpio(BLUE, 4096); 

    //Só é possivel ter uma unica função de interrupção
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &interrupt);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &interrupt);

  while (true)
  {
   adc_select_input(0);
   uint16_t vry_value = adc_read();
   adc_select_input(1); 
   uint16_t vrx_value = adc_read();
   
   
   //O .0 é importante pra indicar que quero um numero fracionario (Caso contrario resulta em 0)
   //O 120 serve pra compensar o fato do display ter eixo Y com sentido pra baixo e o joystick
   //ter o eixo y com sentido pra cima
   teste1=56-(uint8_t)round(56*(vry_value/4095.0));
   teste2=(uint8_t)round(120*(vrx_value/4095.0));

   if(leds_ativos){
    if(teste2 >=54 && teste2 <=60)//Pra compensar a variação da faixa normal quando o Joystick estiver parado
    red_level=0;
    else if(teste2>60){
    red_level= (uint16_t)floor(2.0*(vrx_value-2050));
    if(red_level>=4072) red_level=4096;
    } else{//Caso o Joystick esteja sendo empurrado pra esquerda (<54)
      //red_level=4096-vrx_value;
      red_level=(uint16_t)floor(2.5*(1680-vrx_value));
      if(red_level>4096) red_level=4096;
    }
    pwm_set_gpio_level(RED,red_level);


    if(teste1 >=29 && teste1 <=31)//Pra compensar a variação da faixa normal quando o Joystick estiver parado
    blue_level=0;
    else if(teste1<29){
    blue_level= (uint16_t)floor(2.0*(vry_value-2050));
    if(blue_level>=4072) blue_level=4096;
    } else{//Caso o Joystick esteja sendo empurrado pra baixo
      blue_level=(uint16_t)floor(2.5*(1680-vry_value));
      if(blue_level>4096) blue_level=4096;
    }
    pwm_set_gpio_level(BLUE,blue_level);
  }

   //Coloca limites maximos e minimos pra movimentação do quadrado
   if(teste1 < 4) teste1=4;
   else if(teste1 > 52) teste1=52;

   if(teste2 > 115) teste2=115;
   else if(teste2 < 2) teste2=2;
   
   //current_time = to_ms_since_boot(get_absolute_time());
   //if(current_time - last_time >= 1000){
   //printf("red_level: %u\n",red_level);
   //printf("valor em y: %u\nvalor em x: %u\n\n",teste1,teste2);
   //printf("blue_level: %u\n",blue_level);
   //printf("valor em vry: %u\nvalor em vrx: %u\n\n",vry_value,vrx_value);
   // last_time=current_time;
   //}
   ssd1306_fill(&ssd, !cor); // Limpa o display
   ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
   ssd1306_draw_square(&ssd,teste2,teste1);
   ssd1306_send_data(&ssd);
   sleep_ms(10);
  }
}
