#!/bin/bash

# 1. Fecha o que estiver aberto
pkill raydash_app
sleep 0.3

# 2. Abre o dashboard
~/RayDash/raydash_app &

echo "Aguardando janela..."

for i in {1..20}; do
    # Busca o endereço da janela
    # Usamos o comando 'address' específico para não ter erro
    ADDR=$(hyprctl clients -j | jq -r '.[] | select(.title | contains("RayDash")) | .address')

if [ ! -z "$ADDR" ] && [ "$ADDR" != "null" ]; then
        echo "Janela encontrada: $ADDR. Forçando transferência..."

        # 1. Força a janela a ser flutuante (vital para mover livremente)
        hyprctl dispatch togglefloating address:$ADDR
        
        # 2. Move a janela especificamente para o monitor HDMI-A-1
        # Usar 'movewindow' para um monitor é mais robusto que coordenadas
        hyprctl dispatch movewindowmon HDMI-A-1 address:$ADDR
        
        # 3. Garante que ela vá para o canto superior esquerdo do monitor novo
        hyprctl dispatch movewindowpixel exact 1366 0,address:$ADDR

        # 4. Foca e coloca em Fullscreen
        hyprctl dispatch focuswindow address:$ADDR
        sleep 0.2
        hyprctl dispatch fullscreen 0
        
        echo "Sucesso!"
        exit 0
    fi
