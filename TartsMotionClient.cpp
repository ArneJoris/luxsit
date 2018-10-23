#include <stdio.h>
#include <mosquitto.h>
#include <string>
#include <Tarts.h>
#include <TartsStrings.h>
#include <ncurses.h>
#include <jsoncpp/json/json.h>

#define GATEWAY_ID  "T5YC28"
#define SENSOR_ID   "T5YFLY"

// Tarts on BeagleBoneBlack
#ifdef BB_BLACK_ARCH
    #define GATEWAY_CHANNELS      0xFFFFFFFF // All Channels
    #define GATEWAY_UARTNUMBER    2          //<<your UART Selection>>
    #define GATEWAY_PINACTIVITY   P9_42      //<<your Activity Pin location>>
    #define GATEWAY_PINPCTS       P8_26      //<<your PCTS Pin location>>
    #define GATEWAY_PINPRTS       P8_15      //<<your PRTS Pin location>>
    #define GATEWAY_PINRESET      P8_12      //<<your Reset Pin location>>
#endif

struct mosquitto* mosquittoClient;

char username[256] = "dunstable";
char password[64] = "welcome";

void SensorGatewayPersist_callback(const char* id){
    printf("TARTS-GWP[%s]\n", id);
    //Save Data about this gateway here.  This will all a user to store key values about the current gateway that could be used to resore back too after reset.
    //TartsGateway* gw = Tarts.FindGateway(id);
    //gw.getChannelMask();     //Permitted channels available during a reform. (0xFFFFFFFF -> All channels enabled, 0x000000FF -> Chan[0-7] enabled)
}


void PrintSensorSettings(const char* id) {
    TartsSensorBase* sensor = Tarts.FindSensor(id);
    printf("Sensor type=%d\n", sensor->getSensorType());
    printf("Sensor interval=%d\n", sensor->getReportInterval());
    printf("Sensor link interval=%d\n", sensor->getLinkInterval());
    printf("Sensor retry count=%d\n", sensor->getRetryCount());
    printf("Sensor recovery=%d\n", sensor->getRecovery());
}

void SensorPersist_callback(const char* id){
    printf("TARTS-SENP[%s]\n", id);
    //Save Data about this sensor here.  This will all a user to store key values about the current sensor that could be used to resore back to after reset.
    PrintSensorSettings(id);
}

void SensorGatewayMessage_callback(const char* id, int stringID){
    printf("TARTS-GWM[%s]-%d: %s\n", id, stringID, TartsGatewayStringTable[stringID]);
    if(stringID == 10) printf("ACTIVE - Channel: %d\n", Tarts.FindGateway(GATEWAY_ID)->getOperatingChannel());
}

void SensorMessage_callback(SensorMessage* msg){
    std::string motion = "1";
    printf("TARTS-SEN[%s]: RSSI: %d, dBm, Battery Voltage: %d.%02d VDC, Data: ", msg->ID, msg->RSSI, msg->BatteryVoltage/100, msg->BatteryVoltage%100);
    bool detectedMotion = false;

    for(int i=0; i< msg->DatumCount; i++){
        if(i != 0) printf(" || ");
        printf("%s | %s | %s", msg->DatumList[i].Name, msg->DatumList[i].Value, msg->DatumList[i].FormattedValue);
        if (msg->DatumList[i].Value == motion) {
          detectedMotion = true;
          printf("BEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEP");
          beep();
        }
    }
    printf("\n");
    PrintSensorSettings(msg->ID);

    Json::FastWriter fastWriter;
    Json::Value payload;
    payload["motion"] = detectedMotion;
    std::string message = fastWriter.write(payload);
    const char *cstr = message.c_str();

    mosquitto_publish(mosquittoClient, NULL, "/motion" , strlen(cstr), cstr, 0, false);
}

void SensorLogException_callback(int stringID){
    printf("TARTS-EX[%u] %s\n", stringID, TartsExceptionStringTable[stringID]);
}

int SetupSensor() {
    Tarts.RegisterEvent_GatewayPersist(SensorGatewayPersist_callback);
    Tarts.RegisterEvent_GatewayMessage(SensorGatewayMessage_callback);
    Tarts.RegisterEvent_SensorPersist(SensorPersist_callback);
    Tarts.RegisterEvent_SensorMessage(SensorMessage_callback);
    Tarts.RegisterEvent_LogException(SensorLogException_callback);

    //Register Gateway
    if (!Tarts.RegisterGateway(TartsGateway::Create(GATEWAY_ID, GATEWAY_CHANNELS, GATEWAY_UARTNUMBER, GATEWAY_PINACTIVITY, GATEWAY_PINPCTS, GATEWAY_PINPRTS, GATEWAY_PINRESET))){
        printf("TARTs Gateway Registration Failed!\n");
        return 1;
    } else {
        printf("Registered sensor gateway.\n");
    }

    if (!Tarts.RegisterSensor(GATEWAY_ID, TartsPassiveIR::Create(SENSOR_ID, 2, 102, 2, 2))) {
        Tarts.RemoveGateway(GATEWAY_ID);
        printf("TARTs Sensor Registration Failed!\n");
        return 2;
    } else {
        printf("Registered sensor.\n");
    }

    return 0;
}

void MosqMessage_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if(message->payloadlen){
		printf("%s %s\n", message->topic, message->payload);
	}else{
		printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}

void MosqConnect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if(result){
        fprintf(stderr, "Connect failed\n");
    }
}

void MosqSubscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

void MosqLog_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("%s\n", str);
}


struct mosquitto* SetupMosquittoClient() {
    static struct mosquitto *mosq = NULL;

    int iResult;
    char const* host = "luxsit.solar.gy";
    int port = 8083;
    int keepalive = 60;
    bool cleanSession = true;

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, cleanSession, NULL);
    if ( ! mosq) {
        throw "MOSQUITTO allocation error.";
    }
    mosquitto_log_callback_set(mosq, MosqLog_callback);
    mosquitto_connect_callback_set(mosq, MosqConnect_callback);
    mosquitto_message_callback_set(mosq, MosqMessage_callback);
    mosquitto_subscribe_callback_set(mosq, MosqSubscribe_callback);

    mosquitto_tls_set(mosq, NULL, "/etc/ssl/certs", NULL, NULL,NULL);
    mosquitto_username_pw_set(mosq, username, password);

    iResult = mosquitto_connect(mosq, host, port, keepalive);
    if (iResult != MOSQ_ERR_SUCCESS) {
        printf("Unable to connect to broker. %s\n", mosquitto_strerror(iResult));
    }
  
    printf("Connected to MOSQUITTO.");

    return mosq;
}


int main(void){
    if(SetupSensor() != 0) {
        exit(1);
    }

    mosquittoClient = SetupMosquittoClient();
    mosquitto_loop_start(mosquittoClient);
    
    while(1){
        Tarts.Process();
        TARTS_DELAYMS(100); //Allow System Sleep to occur
    }
}
