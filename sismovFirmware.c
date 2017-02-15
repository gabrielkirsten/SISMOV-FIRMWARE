/*
                 _                             __ _
         ___(_)___ _ __ ___   _____   __  / _(_)_ __ _ __ _____      ____ _ _ __ ___
        / __| / __| '_ ` _ \ / _ \ \ / / | |_| | '__| '_ ` _ \ \ /\ / / _` | '__/ _ \
        \__ \ \__ \ | | | | | (_) \ V /  |  _| | |  | | | | | \ V  V / (_| | | |  __/
        |___/_|___/_| |_| |_|\___/ \_/   |_| |_|_|  |_| |_| |_|\_/\_/ \__,_|_|  \___|

        Autor: Gabriel Kirsten Menezes RA:148298 - 2016
        Universidade Católica Dom Bosco - Engenharia de Computação
        GitHub https://github.com/gabrielkirsten/SISMOV-FIRMWARE
        Compilador: MikroC PRO for PIC v 6.6.3

*/

// ID do dispositivo
#define id_dipositivo_l           1

/********** DEFINICAO DE PORTAS **********/
#define mux_inh                 PORTA.RA2
#define mux_a                   PORTC.RC2
#define mux_b                   PORTA.RA1
#define led_1                   PORTB.RB2
#define pw_key                  PORTC.RC1
#define STATUS                  PORTB.RB3

/************ ENTRADAS DO MUX ************/
#define OBD                     0
#define GPS                     1
#define GPRS                    2

/******** DEFINICAO DE CONSTANTES ********/
#define BUFFER_SIZE             6   // tamanho do buffer de gravacao
#define max_tentativa_sd                2   // tentativas de conexão com o SD CARD
#define max_tentativa_modulo        3   // tentativas de ligar o módulo GPRS/GPS (SIM908)

// Memory Card Chip Select
sfr sbit Mmc_Chip_Select                  at RD3_bit;
sfr sbit Mmc_Chip_Select_Direction        at TRISD3_bit;

/********** CONFIGURAÇÃO DO LCD **********/

// Configura a saída dos Pinos do LCD.
sbit LCD_RS at RD0_bit;
sbit LCD_EN at RD7_bit;
sbit LCD_D7 at RD2_bit;
sbit LCD_D6 at RD4_bit;
sbit LCD_D5 at RD5_bit;
sbit LCD_D4 at RD6_bit;

// Configura a direção dos pinos do LCD.
sbit LCD_RS_Direction at TRISD0_bit;
sbit LCD_EN_Direction at TRISD7_bit;
sbit LCD_D7_Direction at TRISD2_bit;
sbit LCD_D6_Direction at TRISD4_bit;
sbit LCD_D5_Direction at TRISD5_bit;
sbit LCD_D4_Direction at TRISD6_bit;

/********* FIM CONFIGURACAO LCD  *********/

/********** DEFINIÇÃO VARIAVEIS **********/
// Mensagem de Boas Vindas
char                boasVindasl0[]     = "   < Sismov >   ";
char                boasVindasl1[]     = "sismov.gabrielkirsten.com";
// Nome do arquivo salvo no cartão
char                filename[]         = "data.txt";

// Char de configuração do GPRS
char                gprs1[]            = "AT+CGATT?";
char                gprs2[]            = "AT+CIPMUX=0";
char                gprs3[]            = "AT+CSTT=\"claro.com.br\",\"claro\",\"claro\"";
char                gprs4[]            = "AT+CIICR";
char                gprs5[]            = "AT+CIFSR";
char                gprs6[]            = "AT+CIPSTART=\"TCP\",\"gabrielkirsten.com\",\"5000\"";
char                gprs7[]            = "AT+CIPSEND=30";
char                gprs8[]            = "AT+CIPCLOSE";
char                gprs9[]            = "AT+CIPSHUT";

// Solicitações OBD
char                obd1[]             = "010C"; // RPM
char                obd2[]             = "010D"; // VELOCIDADE
char                obd3[]             = "0111"; // POSICAO ACELERADOR

char                namecard[]         = "sismov"; // Nome do cartão após a formatação

char                protocolo[59];  // Variável para armazenamento do protocolo de comunicação

char                recepcao[20];   // Variável para recepção de dados via UART

// Variáveis diversas
short int           tentativas;     // Quantidade de tentativas realizadas
char                error,          // Armazena o retorno de erros diversos
                                        DADO[8],        // Armazena o retorno de um dado lido pela UART
                                        counterUart = 0;// Contador de interrupções do timer
unsigned int        erro,           // Armazena todos os erros encontrados
                                        i;              // Contador diverso
bit                 sdEnable,       // Bit de indicação da disponibilidade de um cartão SD
                                        solicitacaoOK;  // Bit de indicação do sucesso na solicitação via UART
/*
Definição do bits de erro:
        b8 - Erro comando AT na inicialização do SIM908
        b7 - Erro na inicialização do SIM908
        b6 - Erro ao criar novo arquivo no SD
        b5 - Erro formato do cartão
        b4 - Erro na formatação do cartão de memória
        b3 - Cartao de memória formatado
        b2 - Erro na inicialização do SD CARD, Não detectado
        b1 - Erro na inicialização do protocolo SPI
        b0 - Erro na inicialização do protocolo serial USART
*/
/********* FIM DEFINIÇÃO VARIAVEIS ********/

/********** DEFINIÇÃO DE FUNÇÕES **********/

// Função de interrupção para utilização no recebimento de dados
void interrupt(){
        if(INTCON.T0IF){
                if (counterUart >= 100) {
                        Soft_UART_Break();
                        counterUart = 0;
                        solicitacaoOK = 0;
                }else{
                        counterUart++;
                        INTCON.T0IF = 0;
                }
        }
}

// Função que altera as variaveis de controle do MUX
void comuta_mux(int p){
        mux_inh = 1;
        Delay_ms(100);
        if(p == 0){
                mux_a = 0;
                mux_b = 0;
        }else if(p == 1){
                mux_a = 1;
                mux_b = 0;
        }else if(p == 2){
                mux_a = 0;
                mux_b = 1;
        }else if(p == 3){
                mux_a = 1;
                mux_b = 1;
        }
        Delay_ms(100);
        mux_inh = 0;
}

// Função para ligar/delisgar o módulo SIM908
int ligarModulo(int operacao){
        int fimOperacao                = 0;        // Marca o fim do procedimento
        int tentativas                = 0;        // Quantidade de tentativas

        // operacao = 0 - Desliga o Módulo
        // operacao = 1 - Liga o Módulo

        if((STATUS && operacao) || (!STATUS && !operacao)){
                fimOperacao = 1;
        }

    // Enquanto o procedimento não terminar
        while(!fimOperacao && tentativas < max_tentativa_modulo){
                pw_key = 1;                        // Altera o valor de PWRKEY para 1
                Delay_ms(3000);                    // Aguarda 3s
                pw_key = 0;                        // Altera o valor de PWRKEY para 0
                Delay_ms(1000);                    // Aguarda 1s
                if(operacao && STATUS){            // Caso o módulo tenha sido ligado
                        fimOperacao = 1;               // Marca operação como finalizada
                }else if(!operacao && !STATUS){    // Caso o módulo tenha sido desligado
                        fimOperacao = 1;               // Marca operação como finalizada
                }else{                             // Caso a operação tenha falhado
                        tentativas++;                  // Aumenta o contador de tentativa
                        Delay_ms(2000);                // Aguarda 2s até a próxima tentativa
                }
        }

        return fimOperacao;     // fimOperacao = 0 (ERRO_LIGAR_MODULO)
                            // fimOperacao = 1 (Sucesso)
}

// Procedimento que atualiza o erro e exibe no LCD
void atualizaErro(){
        char lcdsaida[4];
        WordToHex(erro, lcdsaida);
        Lcd_Out(2,1, lcdsaida);
}

// Procedimento de impressao de string na UART
void Soft_UART_Write_Text(char *text) {
        unsigned int x = 0;
        for(x = 0; x <= strlen(text); x++) {
                                Soft_UART_Write(text[x]);
        }
        Soft_UART_Write('\n');
        Soft_UART_Write('\r');
}

// Confere a resposta obtida à partir de um comando AT
int confereResposta(char* respostaEsperada){
        int tamanho = strlen(respostaEsperada);
        char c;
        while(tamanho){
                c =  Soft_Uart_Read(&error); // Recebe o caractere
                // Se o caractere for diferente do esperado
                if(c != respostaEsperada[strlen(respostaEsperada) - tamanho])
                        return 0;        // Retorna 0 indicando que encontrou algum erro
                tamanho--;                // percorre o vetor de char
        }
        // Retorna 1 indicando que chegou ao final da função sem identificar erros
        return 1;
}

// Envia um comando AT e verifica a resposta obtida
int enviarComandoAT(char* comandoAT, char* respostaEsperada){
        i = 0;
        // Tenta enviar o comando e conferir a resposta esperada
        while(i++ <= max_tentativa_modulo){
                Soft_UART_Write_Text(comandoAT);                // Envia o comando
                // Se a resposta foi a esperada, retorna sucesso
                if(confereResposta(respostaEsperada))
                        return 1;
        }
        // Tentou o maximo de vezes e não obteve sucesso
        return 0;
}

// Procedimento para solicitação de dados na OBD
void solicitaOBD(char *text, int tam) {
        comuta_mux(OBD);                        // Troca o MUX para saida do OBD
        i = 0;
        Soft_UART_Write_Text(text);        // Envia o comando
        solicitacaoOK = 1;
        Delay_ms(50);
        // Recebe a quantidade predefinida de caracteres
        // Enquanto todos os caracteres anteriores tiverem sucesso
        while(i < 6 && solicitacaoOK){
                INTCON.GIE = 1;                     // Habilita a interrupção global
                INTCON.T0IE = 1;                    // Habilita Timer0 overflow interrupt
                T0CON=0b10000101;                   // Inicia o contador
                counterUart = 0;
                Soft_Uart_Read(&error);   // Armazena a resposta
                INTCON.GIE = 0;                     // Desabilita a interrupção global
                i++;
        }
        i = 0;
        while(i < tam && solicitacaoOK){
                INTCON.GIE = 1;                     // Habilita a interrupção global
                INTCON.T0IE = 1;                    // Habilita Timer0 overflow interrupt
                T0CON=0b10000101;                   // Inicia o contador
                counterUart = 0;
                DADO[i] = Soft_Uart_Read(&error);   // Armazena a resposta
                if(DADO[i] == 0x20){
                        i--;
                }
                INTCON.GIE = 0;                     // Desabilita a interrupção global
                i++;
        }
        DADO[i] = 0x1B;         // Caractere que indica o fim da string
 }

/******* FIM DEFINIÇÃO DE FUNÇÕES ********/

/***************** MAIN ******************/
void main(){
        UCON.USBEN = 0;                                // Disable USB => RC4 + RC5 output
        UCFG.UTRDIS = 1;                        // Disable USB => RC4 + RC5 output
        ADCON1 = 0x0F;                                // Configura os pinos analógicos como digitais
        CMCON  = 7;                                        // Desliga os comparadores
        T0CON = 0x04;                                // Configura o divisor de Clock do Timer 0
        erro = 0x0000;                                // Zera o codigo de erro

        /********** CONFIGURACAO PORTAS **********/
        //DEFINICAO PORTAS A
        TRISA = 0b00000000;
        //DEFINICAO PORTAS B
        TRISB = 0b00101001;
        //DEFINICAO PORTAS C
        TRISC = 0b10000000;
        //DEFINICAO PORTAS D
        TRISD = 0b00000000;

        /******** FIM CONFIGURACAO PORTAS ********/

        pw_key = 0;                // Inicia o power key em nível lógico baixo

        /************* INICIALIZA LCD ************/

        Lcd_Init();                     // Inicializar LCD
        Lcd_Cmd(_LCD_CLEAR);            // Limpar LCD
        Lcd_Cmd(_LCD_CURSOR_OFF);          // Desligar cursor
        Lcd_Out(1,1,boasVindasl0);                // Escrever primeira linha
        Lcd_Out(2,1,boasVindasl1);                // Escrever segunda linha
        Delay_ms(500);                                        // Exibe a mensagem por 5 segundos
        i = 9;
        // Roda a mensagem na linha inferior
        while(i--){
                Lcd_Cmd(_LCD_SHIFT_LEFT);
                Lcd_Out(1,1-(i-9),boasVindasl0);
                Delay_ms(300);
        }
        Delay_ms(2000);
        Lcd_Cmd(_LCD_CLEAR);                        // Limpar LCD
        Lcd_Out(1,1,"Iniciando...");

        //******** FIM INICIALIZA LCD *******//


        //********** INICIALIZA SD **********//
        SPI1_Init_Advanced(_SPI_MASTER_OSC_DIV64, _SPI_DATA_SAMPLE_MIDDLE,
                                        _SPI_CLK_IDLE_LOW, _SPI_LOW_2_HIGH);
        Delay_us(300);
        tentativas, sdEnable = 0;

        // Tenta Iniciar o cartão LCD
        error = MMC_Fat_Init();
        while(error == 255 && tentativas < max_tentativa_sd){
                error = MMC_Fat_Init();
                tentativas++;
                erro = (erro | 0x0004);
        }
        // Verificação da resposta na inicialização
        if(error == 1){ // Erro no formato
                erro = (erro | 0x0020);
                error = Mmc_Fat_QuickFormat(namecard);
                if (error == 1){                // Erro na formatação
                        erro = (erro | 0x0010);
                }else if(error == 0){        // Cartão formatado
                        erro = (erro | 0x0008);
                        error = MMC_Fat_Init();        // Tenta iniciar o cartão
                        while(error == 255 && tentativas < max_tentativa_sd){
                                error = MMC_Fat_Init();
                                tentativas++;
                                erro = (erro | 0x0004);
                        }
                        if(error == 0){
                                // Indica que o cartão foi inicializado com sucesso
                                sdEnable = 1;
                                // Aumenta a velocidade do clock no SPI
                                SPI1_Init_Advanced(_SPI_MASTER_OSC_DIV4, _SPI_DATA_SAMPLE_MIDDLE,
                                                            _SPI_CLK_IDLE_LOW, _SPI_LOW_2_HIGH);
                                // Abre o arquivo concatenando as novas informaçãoes no arquivo
                                error = Mmc_Fat_Assign(&filename,0xA0);
                                if(error == 0 || error == 2){
                                        erro = (erro | 0x0040);
                                }
                        }
                }
        }else if(error == 0){ // Não houve erros no cartão
                sdEnable = 1;
                SPI1_Init_Advanced(_SPI_MASTER_OSC_DIV4, _SPI_DATA_SAMPLE_MIDDLE,
                                                _SPI_CLK_IDLE_LOW, _SPI_LOW_2_HIGH);
        }else if(error == 255){
                erro = (erro | 0x0004);
        }


        /********** FIM INICIALIZA SD *********/

        error = Soft_UART_Init(&PORTB, 5, 4, 9600, 0); // Inicializa a comunicação UART
        if(error != 0){
                erro = erro | 0x0001; // Erro na inicialização da UART
        }

        /************ CONFIGURA GPRS ***********/
        comuta_mux(GPRS);
        Delay_ms(50);

        if(!ligarModulo(1)){
                        erro = erro | 0x0080;
        }

        // Envia os comandos de inicialização para o módulo GPRS

        if(!(erro&0x0180) && !enviarComandoAT(gprs1, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs2, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs3, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs4, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs5, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs6, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs7, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs8, "OK")){
                        erro = erro | 0x0100;
        }
        if(!(erro&0x0180) && !enviarComandoAT(gprs9, "OK")){
                        erro = erro | 0x0100;
        }
        //********** FIM CONFIGURA GPRS *********//

        Lcd_Out(1,1,"Running...  ");

        //********** LOOP INFINITO **********//
        while(1){
                atualizaErro(); // Atualiza o valor do erro no LCD

                // CRIACAO DO PROTOCOLO PARA TROCA DE MENSAGENS

                // ID BITS
                protocolo[0] = (id_dipositivo_l & 0x000000FF);
                protocolo[1] = (id_dipositivo_l & 0x0000FF00);
                protocolo[2] = (id_dipositivo_l & 0x00FF0000);
                protocolo[3] = (id_dipositivo_l & 0xFF000000);
                protocolo[4] = 0x0;
                protocolo[5] = 0x0;
                protocolo[6] = 0x0;
                protocolo[7] = 0x0;

                // RPM BITS
                solicitaOBD(obd1, 4);
                protocolo[8] = DADO[0];                //A
                protocolo[9] = DADO[1];                //A
                protocolo[10] = DADO[2];        //B
                protocolo[11] = DADO[3];        //B

                // VELOCIDADE
                solicitaOBD(obd2, 2);
                protocolo[12] = DADO[0];        //A
                protocolo[13] = DADO[1];        //A

                // THROTTLE POSITION
                solicitaOBD(obd3, 2);
                protocolo[14] = DADO[0];        //A
                protocolo[15] = DADO[1];        //A
                for(i = 16; i<55; i++){
                        protocolo[i] = '0';
                }

                // LATITUDE

                // LONGITUDE

                // ALTITUDE

                // N_SAT


                // CHECKSUN
                protocolo[56] = 0;
                for(i = 0; i<56; i++){
                        protocolo[56] += protocolo[i];
                }

                // Concatena os indicadores de retorno de carro e quebra de linha
                protocolo[57] = '\r';
                protocolo[58] = '\n';

                // Salva os dados no SD Card (somente se disponivel)
                if(sdEnable){
                        error = Mmc_Fat_Assign(&filename, 0xA0);
                        Mmc_Fat_Append();
                        Mmc_Fat_Write(protocolo, 59);
                }

                // Envia os dados para um servidor remoto

                // Pisca o led indicando um ciclo
                led_1 = 1;
                Delay_ms(30);
                led_1 = 0;
                Delay_ms(970);
        }
}