# Instruções para Configuração do Arduino IDE para ESP32

Este guia ajudará você a configurar o Arduino IDE para usar placas ESP32.

---

## Passo a Passo

### 1️⃣ Abrir as Preferências do Arduino IDE

- Abra o **Arduino IDE**.
- Vá para **Arquivo** > **Preferências** (ou **File** > **Preferences**).

### 2️⃣ Adicionar URL do Gerenciador de Placas

- Na seção **URLs adicionais para o Gerenciador de Placas** (ou **Additional Boards Manager URLs**), adicione a seguinte URL:

  > https://dl.espressif.com/dl/package_esp32_index.json

- Se houver outras URLs já adicionadas, separe as URLs com uma vírgula.
- Clique em **OK** para salvar as configurações.

### 3️⃣ Instalar o Suporte para ESP32

- Vá para **Ferramentas** > **Placa** > **Gerenciador de Placas** (ou **Tools** > **Board** > **Boards Manager**).
- No campo de pesquisa, digite **ESP32**.
- Clique em **Instalar** ao lado da opção **esp32 by Espressif Systems**.

### 4️⃣ Selecionar a Placa ESP32

- Vá para **Ferramentas** > **Placa** (ou **Tools** > **Board**) e selecione a placa ESP32 que está usando.

### 5️⃣ Configurar a Placa ESP32 Dev Module

Após selecionar a placa **ESP32 Dev Module**, ajuste as configurações para garantir um bom funcionamento:

1. **Vá para**: **Ferramentas** > **Placa** > **ESP32 Arduino** > **ESP32 Dev Module**  
2. **Configure os parâmetros recomendados**:

   - **CPU Frequency**: `240MHz (WiFi/BT)`  
   - **Flash Frequency**: `80MHz`  
   - **Flash Mode**: `QIO`  
   - **Flash Size**: `4MB (32Mb)`  
   - **Partition Scheme**: `Default 4MB with spiffs`  
   - **Upload Speed**: `115200`  
   - **Core Debug Level**: `None`  
   - **PSRAM**: `Disabled`  

3. **Conecte o ESP32 ao computador via USB** e selecione a porta correta:  
   - Vá em **Ferramentas** > **Porta** e escolha a porta onde o ESP32 está conectado.  
   - No Windows, pode aparecer como **COMx** (exemplo: COM3).  
   - No Linux/macOS, pode ser algo como **/dev/ttyUSB0** ou **/dev/tty.SLAB_USBtoUART**.

4. **Agora você pode carregar seu código no ESP32!** 🚀  

🔹 **Dica**: Se houver problemas de upload, pressione uma vez o botão **BOOT** na placa durante o início do envio do código.

---

## 📚 Adicionar Bibliotecas no Diretório Correto

Se o seu projeto utiliza bibliotecas externas, siga os passos abaixo para adicioná-las corretamente:

1. Copie a pasta `libraries` do projeto.
2. Acesse o diretório de bibliotecas do Arduino:
   - **Windows**: `C:\Users\SEU_USUARIO\Documents\Arduino\libraries`
   - **Linux**: `~/Documentos/Arduino/libraries`
   - **Mac**: `~/Documents/Arduino/libraries`
3. Cole a pasta **dentro do diretório `libraries`**.

🔹 **Observação**: Se o Arduino IDE já estiver aberto, reinicie-o para reconhecer as novas bibliotecas.

---

## 🛠️ Solução de Problemas

- **Placa não aparece no Gerenciador de Placas**  
  ➜ Verifique se a URL está correta e tente novamente.  

- **Bibliotecas não estão sendo reconhecidas**  
  ➜ Confirme se os arquivos estão dentro da pasta correta (`Documents/Arduino/libraries`).  

- **Erro de upload para o ESP32**  
  ➜ Pressione o botão **BOOT** no ESP32 antes do upload do código.  

---

## 🤝 Contribuição

Sinta-se à vontade para contribuir para este repositório se encontrar problemas ou melhorias!

