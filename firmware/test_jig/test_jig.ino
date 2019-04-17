#define b1_cha 12
#define b1_chb 11

unsigned long test_durations[] = {
25
,30
,40
,50
,60
,70
,80
,100
,130
,150
,260
,305};
//select channel,duration,count(*) from testing where duration > 24 group by 1,2 order by 2 ASC 
void setup()
{
        pinMode(b1_cha,OUTPUT);
        pinMode(b1_chb,OUTPUT);
        digitalWrite(b1_cha,HIGH);
        digitalWrite(b1_chb,HIGH);
        
        Serial.begin(115200);
        
        delay(5000);
}

unsigned long last_ms;
byte test_index = 0;
bool is_test = false;
int test_round = 1;
bool ch_b_follow = false; 
void loop()
{
        
        if (is_test && (millis() - last_ms > (test_durations[test_index] * 1000)) )
        {
                Serial.print(" Aoff");
                digitalWrite(b1_cha,HIGH);
                is_test = false;
                last_ms = millis();
                test_index++;
                
                if (test_index > 11)
                {
                        test_index = 0;
                        test_round++;
                }
                
                ch_b_follow = true; 
        }
        
        if (!is_test && (millis() - last_ms > 20000) || last_ms < 1)
        {
                Serial.print("\n");Serial.print("Round: ");Serial.print(test_round);Serial.print(" Test: ");Serial.print(test_index+1);Serial.print(" : ");Serial.print(test_durations[test_index]);Serial.print(" Aon");
                is_test = true;
                digitalWrite(b1_cha,LOW);
                ch_b_follow = true;
                last_ms = millis();
        }
        
        if (ch_b_follow == true && (millis() - last_ms > 10000))
        {
                Serial.print("=>B"); 
                ch_b_follow = false;
                digitalWrite(b1_chb,digitalRead(b1_cha));
        }
        
        
        
}