# AS5
Projeto localizador iss

Este projeto utiliza a library ArduinoP13 para calcular a propagação orbital https://github.com/dl9sec/ArduinoP13

O projeto consistiu em utilizar a biblioteca Wire (I²C) para conectar diversos microcontroladores em uma aplicação qualquer, os quais deveriam trocar dados.
Eu tinha vários componentes como um servo motor e um motor de passo apenas juntando pó, e queria utilizá-los em algum projeto.
Quando a atividade foi apresentada tive a ideia de criar um robô que sempre aponta para a estação espacial quando ligado, e encontrei na internet projetos muito similares ao que tinha em mente (há um no canal Practical Engineering no youtube, por exemplo).

Então aqui está. Vídeo do projeto em funcionamento (Timelapse) https://www.youtube.com/watch?v=MJCZitP32i4&ab_channel=Caiohms

Há também um bug que ocorre aleatóriamente algum tempo (30 min a 3h) depois que o projeto é ligado, creio que seja um bug no próprio bus i2c, a exception ocorre no arquivo freertos/queue.h e ainda não sei como reproduzir o bug.

Pretendo deixar o projeto todo em apenas um ESP32 (ao invés de 2 ESP e 1 Arduino nano como está agora), então corrigir esse bug não será necessário.
