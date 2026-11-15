/*
  시간의 정원 - 적외선 센서 + 무게센서(HX711) → 웹 전시 화면
  ------------------------------------------------------------
  웹 페이지(크롬/엣지)와 USB 시리얼(9600)로 연결됩니다.
  아두이노가 보내는 신호는 딱 두 줄입니다.

    START        모래가 깔때기 관을 지나가기 시작함
    END,12.3     모래가 멈춤. 12.3 = 방금 부은 모래 무게(g)

  (선택) W,123.4 처럼 현재 총무게를 보내면 화면 왼쪽 위에 표시됩니다.

  필요한 라이브러리: "HX711 Arduino Library" (by Bogdan Necula)
  보드: Arduino UNO
*/

#include "HX711.h"

// ---------- 핀 설정 (배선에 맞게 바꾸세요) ----------
const int IR_PIN = 7;              // 적외선 센서 출력 핀
const bool IR_ACTIVE_LOW = true;   // 모래(물체)가 있을 때 LOW가 나오는 센서면 true
const int HX_DT = 3;               // HX711 DT
const int HX_SCK = 2;              // HX711 SCK

// ---------- 조정값 ----------
float CALIBRATION = 420.0;                    // 무게 보정값 (아래 '보정 방법' 참고)
const unsigned long START_CONFIRM_MS = 80;    // 이 시간 이상 감지되면 '흐르기 시작'
const unsigned long END_QUIET_MS = 1500;      // 이 시간 동안 감지가 없으면 '멈춤'
const unsigned long SETTLE_MS = 800;          // 멈춘 뒤 모래가 가라앉을 때까지 기다림
const bool SEND_LIVE_WEIGHT = false;          // true면 1초마다 W,총무게 전송 (테스트용)

HX711 scale;
bool flowing = false;
unsigned long firstSeen = 0, lastSeen = 0, lastLive = 0;
float baseline = 0;   // 붓기 직전의 무게 (영점)

bool sandDetected() {
  int v = digitalRead(IR_PIN);
  return IR_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

void setup() {
  Serial.begin(9600);
  pinMode(IR_PIN, INPUT);
  scale.begin(HX_DT, HX_SCK);
  scale.set_scale(CALIBRATION);
  scale.tare();                      // 처음 켤 때 빈 통 기준으로 영점
  baseline = scale.get_units(10);
  Serial.println("READY");
}

void loop() {
  unsigned long now = millis();
  bool seen = sandDetected();

  if (!flowing) {
    // 기다리는 동안 영점을 천천히 따라감 (통에 모래가 계속 쌓여도 매번 '방금 부은 양'만 잼)
    if (scale.is_ready()) baseline = baseline * 0.9 + scale.get_units(1) * 0.1;

    if (seen) {
      if (firstSeen == 0) firstSeen = now;
      if (now - firstSeen >= START_CONFIRM_MS) {   // 잠깐 스친 잡음은 무시
        flowing = true;
        lastSeen = now;
        Serial.println("START");
      }
    } else {
      firstSeen = 0;
    }
  } else {
    if (seen) lastSeen = now;
    if (now - lastSeen >= END_QUIET_MS) {          // 한동안 감지가 없으면 멈춘 것으로 판단
      delay(SETTLE_MS);
      float grams = scale.get_units(10) - baseline;
      if (grams < 0) grams = 0;
      Serial.print("END,");
      Serial.println(grams, 1);
      baseline = scale.get_units(10);              // 다음 사람을 위한 새 영점
      flowing = false;
      firstSeen = 0;
    }
  }

  if (SEND_LIVE_WEIGHT && now - lastLive >= 1000 && scale.is_ready()) {
    lastLive = now;
    Serial.print("W,");
    Serial.println(scale.get_units(1), 1);
  }
}

/*
  보정 방법
  1) CALIBRATION 을 1.0 으로 바꾸고 업로드, SEND_LIVE_WEIGHT 를 true 로.
  2) 시리얼 모니터(9600)에서 빈 통일 때 값을 확인한 뒤, 무게를 아는 물건(예: 100g)을 올립니다.
  3) (올렸을 때 값 - 빈 통 값) / 100  =  CALIBRATION 값
  4) 그 값을 넣고 다시 업로드하면 W 값이 g 단위로 나옵니다.
  ※ 웹과 연결할 때는 시리얼 모니터를 꼭 닫아 주세요. (포트를 동시에 쓸 수 없어요)
*/
