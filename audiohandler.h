#ifndef AUDIOHANDLER_H
#define AUDIOHANDLER_H

#include <QObject>
#include <QSoundEffect>
#include <QUrl>
#include <QMap>

constexpr const char* MINECRAFT_CLICK_SFX_PATH = "./assets/Sound Effects/ButtonPlate Click (Minecraft Sound).wav";
constexpr const char* EXPERIMENT_BEGINNING_SFX_PATH = "./assets/Sound Effects/Experiment Beginning.wav";
constexpr const char* EXPERIMENT_STOPPED_SFX_PATH = "./assets/Sound Effects/Experiment Stopped.wav";
constexpr const char* TELEMETRY_CONNECTED_SFX_PATH = "./assets/Sound Effects/Telemetry Connected.wav";
constexpr const char* EXPERIMENT_CANCELLED_BY_USER = "./assets/Sound Effects/Experiment Cancelled By User.wav";
constexpr const char* EXPERIMENT_FINISHED_SFX_PATH = "./assets/Sound Effects/Experiment Finished Successfully.wav";
constexpr const char* CURRENT_LIMIT_REACHED_SFX_PATH = "./assets/Sound Effects/Current Limit Reached.wav";
constexpr const char* STABILIZING_THRUST_SFX_PATH = "./assets/Sound Effects/Stabilizing Thrust.wav";
constexpr const char* CHANGED_TO_SERIAL_SFX_PATH = "./assets/Sound Effects/Changed To Serial.wav";
constexpr const char* CHANGED_TO_MQTT_SFX_PATH = "./assets/Sound Effects/Changed To MQTT Wireless.wav";
constexpr const char* TARGET_RPM_REACHED_SFX_PATH = "./assets/Sound Effects/Target RPM Reached.wav";
constexpr const char* CHANGE_TO_HOME_FIRST_SFX_PATH = "./assets/Sound Effects/Please Change To Home Scene First.wav";
constexpr const char* TURNOFF_EXPERIMENT_FIRST_SFX_PATH = "./assets/Sound Effects/Please Turn Off Experiment.wav";
constexpr const char* ADJUSTING_RPM_SFX_PATH = "./assets/Sound Effects/Adjusting RPM.wav";
constexpr const char* ARMED_SFX_PATH = "./assets/Sound Effects/Armed.wav";
constexpr const char* DISARMED_SFX_PATH = "./assets/Sound Effects/Disarmed.wav";

const QMap<QString, QString> audio_paths = {
    {"MainClick", MINECRAFT_CLICK_SFX_PATH},
    {"ExperimentBeginning", EXPERIMENT_BEGINNING_SFX_PATH},
    {"ExperimentStopped", EXPERIMENT_STOPPED_SFX_PATH},
    {"TelemetryConnected", TELEMETRY_CONNECTED_SFX_PATH},
    {"ExperimentCancelled", EXPERIMENT_CANCELLED_BY_USER},
    {"ExperimentFinished", EXPERIMENT_FINISHED_SFX_PATH},
    {"CurrentLimitReached", CURRENT_LIMIT_REACHED_SFX_PATH},
    {"StabilizingThrust", STABILIZING_THRUST_SFX_PATH},
    {"ChangedToSerial", CHANGED_TO_SERIAL_SFX_PATH},
    {"ChangedToMQTT", CHANGED_TO_MQTT_SFX_PATH},
    {"TargetRPMReached", TARGET_RPM_REACHED_SFX_PATH},
    {"ChangeToHomeFirst", CHANGE_TO_HOME_FIRST_SFX_PATH},
    {"TurnOffExperimentFirst", TURNOFF_EXPERIMENT_FIRST_SFX_PATH},
    {"Armed", ARMED_SFX_PATH },
    {"Disarmed", DISARMED_SFX_PATH },
    {"Adjusting RPM", ADJUSTING_RPM_SFX_PATH }
};

class AudioHandler : public QObject {
    Q_OBJECT

public:
    explicit AudioHandler(QObject* parent = nullptr) : QObject(parent), soundOn(true) {
        // пяожоятымы тоус гвоус стг лмглг

        for (auto it = audio_paths.begin(); it != audio_paths.end(); ++it) {
            QSoundEffect* effect = new QSoundEffect(this);
            effect->setSource(QUrl::fromLocalFile(it.value()));
            sound_effects[it.key()] = effect;
        }
    }

    bool isSoundPlaying(const QString& audioKey) {
        if (sound_effects.contains(audioKey)) {
            return sound_effects[audioKey]->isPlaying();
        }
        return false;
    }

    void setSoundStatus(bool enabled) {

        this->soundOn = enabled;
    }

    void playSound(int volume, QString audio) {
        QString real_audio_path = audio_paths[audio];

        if (soundOn && sound_effects.contains(audio) ) {
            QSoundEffect* effect = sound_effects[audio];
            effect->setVolume(volume); 
            effect->play();
        }
        
    }

private:
    QMap<QString, QSoundEffect*> sound_effects;
    bool soundOn;
};

#endif 