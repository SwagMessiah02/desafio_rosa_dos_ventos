#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "hardware/adc.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include <string.h>

#define WIFI_SSID "Online_A1E4"
#define WIFI_PASSWORD "0029966425"
#define SERVER_IP "192.168.1.10" 
#define SERVER_PORT 5000

struct tcp_pcb *client;

struct Pos {
    uint x_pos;
    uint y_pos;
};

static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
void enviar_dados(struct Pos pos);
const char* posicao(struct Pos pos);
void set_pos(struct Pos *pos);
void setup();

int main() {
    setup();

    // Inicia a comunicação via wi-fi
    if (cyw43_arch_init()) {
        printf("Erro ao iniciar Wi-Fi.\n");
        return 1;
    }

    // Habilita o modo estação wi-fi
    cyw43_arch_enable_sta_mode();

    // Tenta iniciar uma conexão com uma rede wi-fi
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Erro ao conectar-se ao Wi-Fi.\n");
        return 1;
    }

    printf("Wi-Fi conectado!\n");

    client = tcp_new();
    if (!client) {
        printf("Erro ao criar PCB TCP.\n");
        return 1;
    }

    ip_addr_t server_ip;
    ipaddr_aton(SERVER_IP, &server_ip);

    // Tenta estabelecer uma conexão TCP
    err_t err = tcp_connect(client, &server_ip, SERVER_PORT, tcp_connected_callback);
    if (err != ERR_OK) {
        printf("Erro ao conectar: %d\n", err);
        return 1;
    }

    sleep_ms(1000); // Aguarda um segundo antes de começar a enviar os dados

    while (true) {
        cyw43_arch_poll();

        struct Pos pos;
        set_pos(&pos);
        enviar_dados(pos);

        sleep_ms(250); 
    }

    cyw43_arch_deinit();

    return 0;
}

void enviar_dados(struct Pos pos) {
    // Armazena no buffer o conteúdo a ser enviado ao servido 
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s\n", strcmp(posicao(pos), " ") == 0 ? " " : posicao(pos));

    // Envia os dados imediatamente ao servidor TCP
    if (strcmp(buffer, " \n") != 0) {
        err_t err = tcp_write(client, buffer, strlen(buffer), TCP_WRITE_FLAG_COPY);

        if(err != ERR_OK) {
            printf("Erro ao tentar enfilheirar dados\n");
            tcp_abort(client);
            return;
        }

        if(tcp_output(client) != ERR_OK) {
            tcp_abort(client);
            return;
        }
    }
}

// Função de callback chamada quando uma conexão TCP for estabelecida com um servidor
static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar-se com o servidor: %d\n", err); 
        return err;
    }

    printf("Conectado ao servidor\n");

    char mensagem[64]; 
    snprintf(mensagem, sizeof(mensagem), "Cliente conectado na porta: %d IP: %s\n", SERVER_PORT, SERVER_IP);
    err_t wr_err = tcp_write(tpcb, mensagem, strlen(mensagem), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    tcp_sent(tpcb, tcp_sent_callback);

    return ERR_OK;
}

// Função de callback para confirma o envio dos dados
static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    printf("%d bytes foram enviados\n", len);
    // tcp_close(tpcb);
    return ERR_OK;
}

// Função para a inicialização dos periféricos
void setup() {
    stdio_init_all();

    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
}

// Atualiza a posição das cordenadas x e y de acordo com a posição do joystick
void set_pos(struct Pos *pos) {
        adc_select_input(1);
        uint16_t adc_x_raw = adc_read();

        adc_select_input(0);
        uint16_t adc_y_raw = adc_read();

        const uint bar_width = 40;
        const uint adc_max = (1 << 12) - 1; 
        uint bar_x_pos = adc_x_raw * bar_width / adc_max;
        uint bar_y_pos = adc_y_raw * bar_width / adc_max;

        pos->x_pos = bar_x_pos;
        pos->y_pos = bar_y_pos;
}

// Retrona a orientação do joystick na rosa dos ventos
const char* posicao(struct Pos pos) {
    if(pos.x_pos == 19 && (pos.y_pos > 19 && pos.y_pos <= 39)) {
        return "Norte";
    } else if(pos.x_pos == 19 && (pos.y_pos < 19 && pos.y_pos >= 0)) {
        return "Sul";
    } else if((pos.x_pos < 19 && pos.x_pos >= 0) && pos.y_pos == 19) {
        return "Oeste";
    } else if((pos.x_pos > 19 && pos.x_pos <= 39) && pos.y_pos == 19) {
        return "Leste";
    } else if((pos.x_pos > 19 && pos.x_pos <= 39) && (pos.y_pos < 19 && pos.y_pos >= 0)) {
        return "Sudeste";
    } else if((pos.x_pos > 19 && pos.x_pos <= 39) && (pos.y_pos > 19 && pos.y_pos <= 39)) {
        return "Nordeste";
    } else if((pos.x_pos < 19 && pos.x_pos >= 0) && (pos.y_pos > 19 && pos.y_pos <= 39)) {
        return "Noroeste";
    } else if((pos.x_pos < 19 && pos.x_pos >= 0) && (pos.y_pos < 19 && pos.y_pos >= 0)) {
        return "Sudoeste";
    } else {
        return " ";
    }
}   