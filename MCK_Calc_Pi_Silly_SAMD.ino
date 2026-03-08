// Note that you need to change SerialUSB to Serial if you are using a non Native serial comms port

float StartTime = 0.0;
float EndTime = 0.0;
float numofTerms = 100000.0;
float pi = 0.0;

void setup() {
  // put your setup code here, to run once:
  SerialUSB.begin(115200);
  while (!SerialUSB)
  SerialUSB.println("Starting...");
}

void loop() {
  // put your main code here, to run repeatedly:
  pi = 0.0;
  StartTime = millis();
  for (float k = 0.0; k <= numofTerms; k++) {
    pi += 4.0 * ( pow((-1.0), k) ) * ( 1.0 / (2.0 * k + 1) );
  }
  EndTime = millis();

  float TotalTime = (EndTime - StartTime)/1000;
  SerialUSB.print("Calculation Took ");
  SerialUSB.print(TotalTime,10);
  SerialUSB.print(" Seconds, and Pi is " );
  SerialUSB.print(pi,10);
  SerialUSB.println(" ish");

}
