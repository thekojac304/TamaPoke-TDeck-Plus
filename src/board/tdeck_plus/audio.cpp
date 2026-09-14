#include "../board_select.h"
#if TAMAPOKE_TDECK_PLUS
#include "../../../audio.h"
#include "pins.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <Preferences.h>

// T-Deck Plus has a direct I2S speaker path (no ES8311 setup). Keep the same
// 16 kHz stereo PCM synthesis and effect IDs as the Waveshare backend.
constexpr uint32_t SAMPLE_RATE = 16000;
static I2SClass i2s;
static QueueHandle_t gQ = nullptr;
static bool gOn = true, gSleeping = false, gFailed = false, gActive = false;
static volatile int gPlaying = -1;

static const char *sfxName(uint8_t id) {
  static const char *const names[SFX_COUNT] = {
    "TAP","EAT","PLAY","HEART","HATCH","EVOLVE","MEDAL","DENY","BYE","LEVEL"
  };
  return id<SFX_COUNT ? names[id] : "INVALID";
}

struct Note { uint16_t f, ms; };
static const Note N_TAP[]    = {{880,35}};
static const Note N_EAT[]    = {{660,45},{0,12},{660,45}};
static const Note N_PLAY[]   = {{784,45},{988,60}};
static const Note N_HEART[]  = {{1047,55},{1319,90}};
static const Note N_HATCH[]  = {{523,80},{659,80},{784,110},{1047,170}};
static const Note N_EVOLVE[] = {{523,80},{659,80},{784,80},{1047,90},{1319,230}};
static const Note N_MEDAL[]  = {{784,70},{0,25},{784,70},{0,25},{1047,200}};
static const Note N_DENY[]   = {{300,110},{200,170}};
static const Note N_BYE[]    = {{784,150},{659,150},{523,280}};
static const Note N_LEVEL[]  = {{784,70},{1047,130}};
struct SfxDef { const Note *notes; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP,1},{N_EAT,3},{N_PLAY,2},{N_HEART,2},{N_HATCH,4},
  {N_EVOLVE,5},{N_MEDAL,5},{N_DENY,2},{N_BYE,3},{N_LEVEL,2},
};

static int16_t pcm[256*2];

static bool startOutput() {
  if (gActive) return true;
  if (gFailed) return false;
  i2s.setPins(TDeckPins::speakerBck,TDeckPins::speakerWs,TDeckPins::speakerDout);
  if (!i2s.begin(I2S_MODE_STD,SAMPLE_RATE,I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO,I2S_STD_SLOT_BOTH)) {
    gFailed=true;
    Serial.println("AUDIO T-Deck I2S init failed; continuing silent");
    return false;
  }
  gActive=true;
  return true;
}

static void stopOutput() {
  if (!gActive) return;
  // End every burst at zero, then release I2S. This avoids a DC edge/click and
  // leaves no continuously clocked noisy speaker path while the game is idle.
  memset(pcm,0,sizeof(pcm));
  i2s.write((uint8_t *)pcm,sizeof(pcm));
  i2s.write((uint8_t *)pcm,sizeof(pcm));
  i2s.end();
  gActive=false;
}

static void playTone(uint16_t f,uint16_t ms) {
  int total=SAMPLE_RATE*ms/1000;
  int half=f ? SAMPLE_RATE/(2*f) : 0;
  constexpr int16_t amp=5000;
  int phase=0,done=0; bool high=true;
  while (done<total && gOn && !gSleeping) {
    int n=total-done; if (n>256) n=256;
    for (int i=0;i<n;++i) {
      int16_t sample=0;
      if (f) {
        sample=high ? amp : -amp;
        int idx=done+i;
        if (idx<64) sample=(int16_t)(sample*idx/64);
        else if (idx>total-96) sample=(int16_t)(sample*(total-idx)/96);
        if (++phase>=half) { phase=0; high=!high; }
      }
      pcm[i*2]=sample; pcm[i*2+1]=sample;
    }
    i2s.write((uint8_t *)pcm,n*4);
    done+=n;
  }
}

static void playEffect(uint8_t id) {
  if (id>=SFX_COUNT || !startOutput()) return;
  gPlaying=id;
  Serial.printf("AUDIO started id=%u name=%s\n",id,sfxName(id));
  const SfxDef &effect=SFX[id];
  for (uint8_t i=0;i<effect.len && gOn && !gSleeping;++i)
    playTone(effect.notes[i].f,effect.notes[i].ms);
  if (gOn && !gSleeping)
    Serial.printf("AUDIO completed id=%u name=%s\n",id,sfxName(id));
  else
    Serial.printf("AUDIO cancelled id=%u name=%s reason=%s\n",id,sfxName(id),gOn?"sleep":"muted");
  gPlaying=-1;
}

static void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (!xQueueReceive(gQ,&id,portMAX_DELAY)) continue;
    if (gOn && !gSleeping) playEffect(id);
    // Consume effects already queued as one I2S burst. A short grace period
    // keeps rapid minigame sounds continuous without keeping the bus alive.
    while (gOn && !gSleeping && xQueueReceive(gQ,&id,pdMS_TO_TICKS(12)))
      playEffect(id);
    stopOutput();
  }
}

void audioBegin() {
  Preferences prefs;
  prefs.begin("tamapoke",true);
  gOn=prefs.getBool("snd",true);
  prefs.end();
  gQ=xQueueCreate(8,sizeof(uint8_t));
  if (!gQ || xTaskCreatePinnedToCore(audioTask,"audio",4096,nullptr,1,nullptr,0)!=pdPASS) {
    gFailed=true;
    Serial.println("AUDIO T-Deck task init failed; continuing silent");
    return;
  }
  Serial.printf("AUDIO T-Deck I2S BCK=%d WS=%d DOUT=%d idle-off\n",
                TDeckPins::speakerBck,TDeckPins::speakerWs,TDeckPins::speakerDout);
  sfxPlay(SFX_HATCH);
}

void sfxPlay(uint8_t id) {
  if (!gQ || !gOn || gSleeping || gFailed || id>=SFX_COUNT) {
    Serial.printf("AUDIO discarded id=%u name=%s reason=%s\n",id,sfxName(id),
                  !gOn?"muted":gSleeping?"sleep":gFailed?"backend":"invalid");
    return;
  }
  if (xQueueSend(gQ,&id,0)==pdTRUE)
    Serial.printf("AUDIO queued id=%u name=%s\n",id,sfxName(id));
  else
    Serial.printf("AUDIO discarded id=%u name=%s reason=queue-full\n",id,sfxName(id));
}

void audioSetEnabled(bool on) {
  gOn=on;
  if (!on && gQ) {
    UBaseType_t queued=uxQueueMessagesWaiting(gQ);
    xQueueReset(gQ);
    Serial.printf("AUDIO cancelled current=%d queued=%u reason=muted\n",gPlaying,(unsigned)queued);
  }
  Preferences prefs;
  prefs.begin("tamapoke",false);
  prefs.putBool("snd",on);
  prefs.end();
}

bool audioEnabled() { return gOn; }

void audioSetSleeping(bool sleeping) {
  if (gSleeping==sleeping) return;
  gSleeping=sleeping;
  if (sleeping && gQ) {
    UBaseType_t queued=uxQueueMessagesWaiting(gQ);
    xQueueReset(gQ);
    if (gPlaying>=0 || queued)
      Serial.printf("AUDIO cancelled current=%d queued=%u reason=sleep\n",gPlaying,(unsigned)queued);
  }
}
#endif
