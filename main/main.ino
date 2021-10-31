#include <M5StickCPlus.h>
#include <MHZ19_uart.h>
MHZ19_uart mhz19;

// Co2センサー
#define RX_PIN 33	  // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 32	  // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600 // Device to MH-Z19 Serial baudrate (should not be changed)

#define BRIGHTNESS 8

bool lcdOn = true;
bool ledOn = false;
bool ledValue = false;
unsigned long nextLedOperationTime = 0;
unsigned long get_graph_timer = 0;
unsigned long getViewDataTimer = 0;

int history[240] = {};
int historyPos = 0;

float co2_tone_on_limit = 2000.0; // トーンを鳴らすco2の閾値
unsigned long renderTime = 2000;  // 描画感覚のミリ秒

//TFT_eSPI liblary
TFT_eSprite Lcd_buff = TFT_eSprite(&M5.Lcd); // LCDのスプライト表示ライブラリ
void setup()
{
	M5.begin(true, true, true); // LCDEnable, PowerEnable, SerialEnable(115200)
	// dacWrite(25, 0); // Speaker OFF
	M5.Axp.ScreenBreath(BRIGHTNESS);
	pinMode(M5_LED, OUTPUT);
	M5.Lcd.setRotation(1);
	Serial.begin(BAUDRATE); // Device to serial monitor feedback

	//TFT_eSPI setup
	Lcd_buff.createSprite(m5.Lcd.width(), m5.Lcd.height());
	Lcd_buff.fillRect(0, 0, m5.Lcd.width(), m5.Lcd.height(), TFT_BLACK);
	Lcd_buff.pushSprite(0, 0);

	// CO2センサー初期化
	mhz19.begin(RX_PIN, TX_PIN);
	mhz19.setAutoCalibration(true);

	render();
}

void loop()
{
	auto now = millis();

	// Aボタン: モードを切り替える
	M5.update();
	M5.Beep.update(); // tone関数で鳴らした音が指定時間経過していたら止める
	if (M5.BtnA.wasPressed())
	{
	}

	// Aボタン長押し: ゼロキャリブレーション
	if (M5.BtnA.pressedFor(2000))
	{
		mhz19.calibrateZero();
		M5.Beep.tone(1000, 100);
	}

	// Bボタン: 画面表示を ON/OFF する
	if (M5.BtnB.wasPressed())
	{
		lcdOn = !lcdOn;

		if (lcdOn)
		{
			render();
			M5.Axp.ScreenBreath(BRIGHTNESS);
		}
		else
		{
			M5.Axp.ScreenBreath(0);
		}
	}

	if ((ledOn || !ledValue) && nextLedOperationTime <= now)
	{
		ledValue = !ledValue;
		digitalWrite(M5_LED, ledValue);
		M5.Beep.tone(4000, 1000);

		// Lチカ
		if (ledValue)
		{
			nextLedOperationTime = now + 15000;
		}
		else
		{
			nextLedOperationTime = now + 800;
		}
	}

	if (now - getViewDataTimer >= renderTime)
	{
		render();
		getViewDataTimer = now;
	}

	if (now - get_graph_timer >= 180000) // 3分ごとにグラフへ追加
	{
		/* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even
		if below background CO2 levels or above range (useful to validate sensor). You can use the
		usual documented command with getCO2(false) */
		int CO2 = mhz19.getCO2PPM();
		// 測定結果の記録
		historyPos = (historyPos + 1) % (sizeof(history) / sizeof(int));
		history[historyPos] = CO2;

		get_graph_timer = now;
	}
}

void render()
{
	// Clear
	Lcd_buff.fillSprite(TFT_BLACK); // 画面を黒塗りでリセット

	int CO2 = mhz19.getCO2PPM();
	ledOn = CO2 >= co2_tone_on_limit; // LEDをON

	if (!lcdOn) // 描画OFFであれば処理を抜ける
	{
		return;
	}

	int height = m5.Lcd.height();
	int width = m5.Lcd.width();
	int len = sizeof(history) / sizeof(int);
	for (int i = 0; i < len; i++)
	{
		auto value = max(0, history[(historyPos + 1 + i) % len] - 400);
		auto y = min(height, (int)(value * (height / co2_tone_on_limit)));
		auto color = min(255, (int)(value * (255 / co2_tone_on_limit)));
		Lcd_buff.drawLine(i, height - y, i, height, Lcd_buff.color565(255, 255 - color, 255 - color));
	}
	int temp = mhz19.getTemperature();
	Serial.println("CO2 (ppm): " + (String)CO2 + ", Temperature (C): " + (String)temp);
	co2_text_render("CO2 : " + (String)CO2 + " ppm");
	Lcd_buff.pushSprite(0, 0); // LCDに描画
}

void co2_text_render(const String &string)
{
	Lcd_buff.setTextSize(2);
	Lcd_buff.setTextColor(TFT_BLACK);
	Lcd_buff.drawString(string, 11, 11, 2);
	Lcd_buff.drawString(string, 9, 9, 2);

	Lcd_buff.setTextColor(TFT_LIGHTGREY);
	Lcd_buff.drawString(string, 10, 10, 2);

}