/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Nombre: ZXProccesor.h
    
    Creado por:
      Copyright (c) Antonio Tamairón. 2023  / https://github.com/hash6iron/powadcr
      @hash6iron / https://powagames.itch.io/
    
    Descripción:
    Clase que implementa metodos y parametrizado para todas las señales que necesita el ZX Spectrum para la carga de programas.

    NOTA: Esta clase necesita de la libreria Audio-kit de Phil Schatzmann - https://github.com/pschatzmann/arduino-audiokit
    
    Version: 1.0

    Historico de versiones


    Derechos de autor y distribución
    --------------------------------
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
    
    To Contact the dev team you can write to hash6iron@gmail.com
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

//#include <stdint.h>
#include <math.h>

#pragma once

// Clase para generar todo el conjunto de señales que necesita el ZX Spectrum
class ZXProcessor 
{
   
    private:

    HMI _hmi;
    
    // Definición de variables internas y constantes
    uint8_t buffer[0];
      
    //const double sampleDuration = (1.0 / SAMPLING_RATE); //0.0000002267; //
                                                       // segundos para 44.1HKz
    
    // Estos valores definen las señales. Otros para el flanco negativo
    // provocan problemas de lectura en el Spectrum.
    double maxAmplitude = LEVELUP;
    double minAmplitude = LEVELDOWN;

    // Al poner 2 canales - Falla. Solucionar
    const int channels = 2;  
    //const double speakerOutPower = 0.002;

    public:
    // Parametrizado para el ZX Spectrum - Timming de la ROM
    // Esto es un factor para el calculo de la duración de onda
    const double alpha = 4.0;
    // Este es un factor de división para la amplitud del flanco terminador
    const double amplitudeFactor = 1.0;
    // T-states del ancho del terminador
    const int maxTerminatorWidth = 3500; //Minimo debe ser 1ms (3500 TStates)
    // otros parámetros
    const double freqCPU = DfreqCPU;
    const double tState = (1.0 / freqCPU); //0.00000028571 --> segundos Z80 
                                          //T-State period (1 / 3.5MHz)
    int SYNC1 = DSYNC1;
    int SYNC2 = DSYNC2;
    int BIT_0 = DBIT_0;
    int BIT_1 = DBIT_1;
    int PILOT_PULSE_LEN = DPILOT_LEN;
    // int PILOT_TONE = DPILOT_TONE;

    //int PULSE_PILOT_DURATION = PILOT_PULSE_LEN * PILOT_TONE;
    //int PULSE_PILOT_DURATION = PILOT_PULSE_LEN * DPILOT_DATA;

    double silent = DSILENT;
    double m_time = 0;

    private:

        uint8_t _mask_last_byte = 8;

        AudioKit m_kit;

        bool stopOrPauseRequest()
        {
            if (LOADING_STATE == 1)
            {
                if (STOP==true)
                {
                    LOADING_STATE = 2; // Parada del bloque actual
                    ACU_ERROR = 0;
                    return true;
                }
                else if (PAUSE==true)
                {
                    LOADING_STATE = 3; // Parada del bloque actual
                    ACU_ERROR = 0;
                    return true;
                }
            }      

            return false;   
        }

        void insertSamplesError(int samples, bool changeNextEARedge)
        {
            // Este procedimiento permite insertar en la señal
            // las muestras acumuladas por error generado en la conversión
            // de double a int

            // El buffer se dimensiona para 16 bits
            uint8_t buffer[samples*2*channels];
            uint16_t *ptr = (uint16_t*)buffer;

            size_t result = 0;
            uint16_t sample_R = 0;
            uint16_t sample_L = 0;

            sample_R = getChannelAmplitude(changeNextEARedge,true) * (MAIN_VOL_R / 100);
            sample_L = getChannelAmplitude(changeNextEARedge,true) * (MAIN_VOL_L / 100); 

            // Escribimos el tren de pulsos en el procesador de Audio
            // Generamos la señal en el buffer del chip de audio.
            for (int j=0;j<samples;j++)
            {
                //R-OUT
                *ptr++ = sample_R;
                //L-OUT
                *ptr++ = sample_L * EN_STEREO;
                result+=2*channels;

                if (stopOrPauseRequest())
                {
                    // Salimos
                    break;
                }
            }

            m_kit.write(buffer, result);

            // Reiniciamos
            ACU_ERROR = 0;
        }

        double getChannelAmplitude(bool changeNextEARedge, bool isError=false)
        {
            double A = 0;

            // Si está seleccionada la opción de nivel bajo a 0
            // cambiamos el low_level amplitude
            if (ZEROLEVEL)
            {
                minAmplitude = 0;
            }
            else
            {
                minAmplitude = maxLevelDown;
            }

            // Esta rutina genera el pulso dependiendo de como es el ultimo
            if(isError)
            {
                if (LAST_EAR_IS==down)
                {
                    // Hacemos el edge de down --> up               
                    // ¿El próximo flanco se cambiará?
                    A=minAmplitude;
                    LAST_EAR_IS = down;
                }
                else
                {
                    A=maxAmplitude;
                    LAST_EAR_IS = up;
                }
            }
            else
            {
                if (LAST_EAR_IS==down)
                {
                    // Hacemos el edge de down --> up               
                    // ¿El próximo flanco se cambiará?
                    if (changeNextEARedge)
                    {
                        A=maxAmplitude;
                        LAST_EAR_IS = up;
                    }
                    else
                    {
                        A=minAmplitude;
                        LAST_EAR_IS = down;
                    }
                }
                else
                {
                    // Hacemos el edge de up --> downs
                    if (changeNextEARedge)
                    {
                        A=minAmplitude;
                        LAST_EAR_IS = down;
                    }
                    else
                    {
                        A=maxAmplitude;
                        LAST_EAR_IS = up;
                    }
                }
            }
            // Asignamos el nivel de amplitud que le corresponde a cada canal.
            // Vemos si hay inversion de onda  



            // if(INVERSETRAIN)
            // {
            //     if(A==maxAmplitude)
            //     {
            //         A=minAmplitude;
            //     }
            //     else
            //     {
            //         A=maxAmplitude;
            //     }
            // }           

            return(A);
        }

        void createPulse(int width, int bytes, uint16_t sample_R, uint16_t sample_L)
        {
                size_t result = 0;                
                uint8_t buffer[bytes+4];                
                int16_t *ptr = (int16_t*)buffer;
                int chn = channels;           

                for (int j=0;j<width;j++)
                {
                    if (sample_R > 0)
                    {
                        SCOPE=up;
                    }
                    else
                    {
                        SCOPE=down;
                    }
                    
                    //R-OUT
                    *ptr++ = sample_R;
                    //L-OUT
                    *ptr++ = sample_L * EN_STEREO;
                    //                        

                    result+=2*chn;                    
                }            

                // Volcamos en el buffer
                m_kit.write(buffer, result);                
        }

        void semiPulse(int width, bool changeNextEARedge = true)
        {
            // Calculamos el tamaño del buffer
            int bytes = 0; //Cada muestra ocupa 2 bytes (16 bits)
            int chn = channels;
            int frames = 0;
            double frames_rest = 0;

            // El buffer se dimensiona para 16 bits
            int16_t sample_L = 0;
            int16_t sample_R = 0;
            // Amplitud de la señal
            double amplitude = 0;
            
            double rsamples = (width / freqCPU) * SAMPLING_RATE;
            // Ajustamos
            int samples = (rsamples);

            ACU_ERROR = 0;

            //
            // Generamos el semi-pulso
            //

            // Obtenemos la amplitud de la señal según la configuración de polarización,
            // nivel low, etc.
            amplitude = getChannelAmplitude(changeNextEARedge);

            // Ajustamos el volumen
            sample_R = amplitude * (MAIN_VOL_R / 100);
            sample_L = amplitude * (MAIN_VOL_L / 100);

            // Pasamos los datos para el modo DEBUG
            DEBUG_AMP_R = sample_R;
            DEBUG_AMP_L = sample_L;
          
            // Definiciones el número samples para el splitter
            int minFrame = 256;
 
            // Si el semi-pulso tiene un numero 
            // menor de muestras que el minFrame, entonces
            // el minFrame será ese numero de muestras
            if (samples <= minFrame)
            {

                #ifdef DEBUGMODE
                    // log("SIMPLE PULSE");
                    // log(" --> samp:  " + String(samples));
                    // log(" --> bytes: " + String(bytes));
                    // log(" --> Chns:  " + String(channels));
                    // log(" --> T0:  " + String(T0));
                    // log(" --> T1:  " + String(T1));
                #endif 
                // Definimos el tamaño del buffer
                bytes = samples * 2 * channels;
                // Generamos la onda
                createPulse(samples,bytes,sample_R,sample_L);
            }
            else
            {
                // Caso de un silencio o pulso muy largo
                // -------------------------------------

                // Definimos los terminadores de cada frame para el splitter
                int frameSlot = minFrame;      

                // Definimos el buffer
                bytes = minFrame * 2 * channels;

                // Dividimos la onda en trozos
                int framesCounter = 0;

                while(frameSlot <= samples)
                {        

                    createPulse(minFrame, bytes, sample_R, sample_L);
                    frameSlot += minFrame;

                    if (stopOrPauseRequest())
                    {
                        // Salimos
                        break;
                    }

                    framesCounter++;

                }

                // Acumulamos el error producido
                ACU_ERROR = (samples - (minFrame*framesCounter));
                
                #ifdef DEBUGMODE
                    //log("ACU_ERROR: "+ String(ACU_ERROR));
                #endif

            }

            if (stopOrPauseRequest())
            {
                // Salimos
                return;
            }
        }

        double terminator(int width)
        {
            // Vemos como es el último bit MSB es la posición 0, el ultimo bit
            
            // Metemos un pulso de cambio de estado
            // para asegurar el cambio de flanco alto->bajo, del ultimo bit
            // Obtenemos los samples
            semiPulse(width,true);
            
            double rsamples = (width / freqCPU) * SAMPLING_RATE;
            return rsamples;
        }

        void customPilotTone(int lenPulse, int numPulses)
        {
            //
            // Esto se usa para el ID 0x13 del TZX
            //
                   
            #ifdef DEBUGMODE
                log("..Custom pilot tone");
                log("  --> lenPulse:  " + String(lenPulse));
                log("  --> numPulses: " + String(numPulses));
            #endif                

            for (int i = 0; i < numPulses;i++)
            {
                // Enviamos semi-pulsos alternando el cambio de flanco
                semiPulse(lenPulse,true);

                if (stopOrPauseRequest())
                {
                    // Salimos
                    return;
                }

            }
        }

        void pilotTone(int lenpulse, int numpulses)
        {
            // Tono guía para bloque TZX ID 0x10 y TAP

            // Calculamos la frecuencia del tono guía.
            // Hay que tener en cuenta que los T-States dados son de un SEMI-PULSO
            // es decir de la mitad del periodo. Entonces hay que calcular
            // el periodo completo que es 2 * T
            // double freq = (1 / (PILOT_PULSE_LEN * tState)) / 2;   
            // generateWaveDuration(freq, duration, SAMPLING_RATE);            
            
            customPilotTone(lenpulse, numpulses);
        }
        
        void zeroTone()
        {
            // Procedimiento que genera un bit "0"  
            semiPulse(BIT_0, true);
            semiPulse(BIT_0, true);
        }
        
        void oneTone()
        {
            // Procedimiento que genera un bit "1"
            semiPulse(BIT_1, true);
            semiPulse(BIT_1, true);      
        }

        void syncTone(int nTStates)
        {
            // Procedimiento que genera un pulso de sincronismo, según los
            // T-States pasados por parámetro.
            //
            // El ZX Spectrum tiene dos tipo de sincronismo, uno al finalizar el tono piloto
            // y otro al final de la recepción de los datos, que serán SYNC1 y SYNC2 respectivamente.

            semiPulse(nTStates,true);      
        }
      
      

        void sendDataArray(uint8_t* data, int size, bool isThelastDataPart)
        {
            uint8_t _mask = 8;   // Para el last_byte
            uint8_t bRead = 0x00;
            int bytes_in_this_block = 0;     

            // Procedimiento para enviar datos desde un array.
            // si estamos reproduciendo, nos mantenemos.
            if (LOADING_STATE==1 || TEST_RUNNING)
            {

                // Recorremos todo el vector de bytes leidos para reproducirlos
                for (int i = 0; i < size;i++)
                {
                
                    if (!TEST_RUNNING)
                    {
                        // Informacion para la barra de progreso
                        PROGRESS_BAR_BLOCK_VALUE = (int)(((i+1)*100)/(size));

                        if (BYTES_LOADED > BYTES_TOBE_LOAD)
                        {BYTES_LOADED = BYTES_TOBE_LOAD;}
                        // Informacion para la barra de progreso total
                        PROGRESS_BAR_TOTAL_VALUE = (int)((BYTES_LOADED*100)/(BYTES_TOBE_LOAD));
                        
                        if (stopOrPauseRequest())
                        {
                            // Salimos
                            i=size;
                            return;
                        }

                    }


                    // Vamos a ir leyendo los bytes y generando el sonido
                    bRead = data[i];


                    //log("Dato: " + String(i) + " - " + String(data[i]));

                    // Para la protección con mascara ID 0x11 - 0x0C
                    // ---------------------------------------------
                    // "Used bits in the last uint8_t (other bits should be 0) {8}
                    //(e.g. if this is 6, then the bits used (x) in the last uint8_t are: xxxxxx00, wh///ere MSb is the leftmost bit, LSb is the rightmost bit)"
                    
                    // ¿Es el ultimo BYTE?. Si se ha aplicado mascara entonces
                    // se modifica el numero de bits a transmitir

                    if (LOADING_STATE==1 || TEST_RUNNING)
                    {
                        // Vemos si es el ultimo byte de la ultima partición de datos del bloque
                        if ((i == size-1) && isThelastDataPart)
                        {
                            // Aplicamos la mascara
                            _mask = _mask_last_byte;
                            //log("Mascara: " + String(_mask) + " - Dato: [" + String(i) + "] - " + String(data[i]));
                        }
                        else
                        {
                            // En otro caso todo el byte es valido.
                            // se anula la mascara.
                            _mask = 8;
                        }
                    
                        // Ahora vamos a descomponer el byte leido
                        // y le aplicamos la mascara. Es decir SOLO SE TRANSMITE el nº de bits que indica
                        // la mascara, para el último byte del bloque

                        for (int n=0;n < _mask;n++)
                        {
                            // Obtenemos el bit a transmitir
                            uint8_t bitMasked = bitRead(bRead, 7-n);

                            // Si el bit leido del BYTE es un "1"
                            if(bitMasked == 1)
                            {
                                // Procesamos "1"
                                oneTone();
                            }
                            else
                            {
                                // En otro caso
                                // procesamos "0"
                                zeroTone();
                            }
                        }

                        // Hemos cargado +1 byte. Seguimos
                        if (!TEST_RUNNING)
                        {
                            BYTES_LOADED++;
                            bytes_in_this_block++;
                            BYTES_LAST_BLOCK = bytes_in_this_block;              
                        }
                    }
                    else
                    {
                        return;
                    }
                }
                
                // Esto lo hacemos para asegurarnos que la barra se llena entera
                if (BYTES_LOADED > BYTES_TOBE_LOAD)
                {BYTES_LOADED = BYTES_TOBE_LOAD;}

            }       
        }


    public:

        void set_maskLastByte(uint8_t mask)
        {
            _mask_last_byte = mask;
        }

        void silence(double duration)
        {
            LAST_SILENCE_DURATION = duration;

            // Paso la duración a T-States
            double parts = 0, fractPart, intPart, terminator_samples = 0;
            int lastPart = 0; 
            int tStateSilence = 0;  
            int tStateSilenceOri = 0;     
            double samples = 0;     

                logln("Silencio: " + String(duration) + " ms");
                       

            // El silencio siempre acaba en un pulso de nivel bajo
            // Si no hay silencio, se pasas tres kilos del silencio y salimos
            if (duration > 0)
            {
                // tStateSilenceOri = tStateSilence;
                // tStateSilence = (duration / OneSecondTo_ms) * freqCPU;    

                samples = (duration / 1000.0) * SAMPLING_RATE;

                // Esto lo hacemos para acabar bien un ultimo flanco en down.
                // Hay que tener en cuenta que el terminador se quita del tiempo de PAUSA
                // la pausa puede ir desde 0 ms a 0x9999 ms
                // 1ms = 3500 T-States
                //
                // Según la especificación al menos 1ms debe estar en un flanco contrario al ultimo flanco para reconocer este ultimo, y después aplicar
                // el resto, como pausa.
                //

                if (APPLY_END)
                {
                    if (LAST_EAR_IS==down)
                    {
                        // El primer milisegundo es el contrario al ultimo flanco
                        // el terminador se genera en base al ultimo flanco que indique
                        // LAST_EAR_IS
                        #ifdef DEBUGMODE
                            log("Añado TERMINATOR +1ms");
                        #endif

                        terminator_samples = terminator(maxTerminatorWidth);
                        
                        // El terminador ocupa 1ms
                        if (duration > 1)
                        {
                            // Si es mayor de 1ms, entonces se lo restamos.
                            samples -= terminator_samples;
                            // Inicializamos la polarización de la señal
                            //LAST_EAR_IS = down;   
                        }
                    }
                }

                #ifdef DEBUGMODE
                    log("Samples: " + String(samples));
                #endif


                // Aplicamos ahora el silencio
                double width = (samples / SAMPLING_RATE) * freqCPU;

                semiPulse(width, true); 
                
                
                #ifdef DEBUGMODE
                    log("..ACU_ERROR: " + String(ACU_ERROR));
                #endif

                //insertamos el error de silencio calculado 
                insertSamplesError(ACU_ERROR,true);

            }       
        }

        void playPureTone(int lenPulse, int numPulses)
        {
            // syncronize with short leader tone
            customPilotTone(lenPulse, numPulses);          
        }

        void playCustomSequence(int* data, int numPulses)
        {
            //
            // Esto lo usamos para el PULSE_SEQUENCE ID-13
            //

            double samples = 0;
            double rsamples = 0; 

            for (int i = 0; i < numPulses;i++)
            {
                // Generamos los semipulsos        
                semiPulse(data[i],true);
            }

        }   

        void playData(uint8_t* bBlock, int lenBlock, int pulse_len, int num_pulses)
        {
            //
            // Este procedimiento es usado por TAP
            //

            // Inicializamos el estado de la progress bar de bloque.
            PROGRESS_BAR_BLOCK_VALUE = 0;

            // Put now code block
            // syncronize with short leader tone
            pilotTone(pulse_len, num_pulses);
            //log("Pilot tone");
            if (LOADING_STATE == 2)
            {return;}

            // syncronization for end short leader tone
            syncTone(SYNC1);
            //log("SYNC 1");
            if (LOADING_STATE == 2)
            {return;}

            syncTone(SYNC2);
            //log("SYNC 2");
            if (LOADING_STATE == 2)
            {return;}

            // Send data
            //sendDataArray(bBlock, lenBlock);
            sendDataArray(bBlock, lenBlock,true);
            //log("Send DATA");
            if (LOADING_STATE == 2)
            {return;}
                        
            // Silent tone
            silence(silent);

            //log("Send SILENCE");
            if (LOADING_STATE == 2)
            {return;}
            
            //log("Fin del PLAY");
        }

        void playPureData(uint8_t* bBlock, int lenBlock)
        {
            // Se usa para reproducir los datos del ultimo bloque o bloque completo sin particionar, ID 0x14.
            // por tanto deben meter un terminador al final.
            sendDataArray(bBlock, lenBlock,true);

            if (LOADING_STATE == 2)
            {
                return;
            }

            // Ahora enviamos el silencio, si aplica.
            if (silent!=0)
            {
                // Silent tone
                silence(silent);

                if (LOADING_STATE == 2)
                {
                    return;
                }
            }
        }             

        void playDataPartition(uint8_t* bBlock, int lenBlock)
        {
            // Se envia a reproducir el bloque sin terminador ni silencio
            // ya que es parte de un bloque completo que ha sido partido en varios
            sendDataArray(bBlock, lenBlock, false);

            if (LOADING_STATE == 2)
            {
                return;
            }
        } 

        void playDataBegin(uint8_t* bBlock, int lenBlock, int pulse_len ,int num_pulses)
        {
            // PROGRAM
            //double duration = tState * pulse_pilot_duration;
            // syncronize with short leader tone

            pilotTone(pulse_len,num_pulses);
            // syncronization for end short leader tone
            syncTone(SYNC1);
            syncTone(SYNC2);

            // Send data
            sendDataArray(bBlock, lenBlock,false);
                   
        }

        void playDataEnd(uint8_t* bBlock, int lenBlock)
        {
            // Send data
            sendDataArray(bBlock, lenBlock,true);        
            
            // Silent tone
            silence(silent);
        }

        void closeBlock()
        {
            // Añadimos un silencio de 3.5M T-States (1s)
            #ifdef DEBUGMODE
                log("Add END BLOCK SILENCE");
            #endif
            silence(3000);
        }

        void set_ESP32kit(AudioKit kit)
        { 
          m_kit = kit;
        }

        void set_HMI(HMI hmi)
        {
          _hmi = hmi;
        }

        size_t createTestSignal(uint8_t *buffer, int samples, int amplitude)
        {

            // Procedimiento para genera un pulso 
            int chn = channels;
            size_t result = 0;
            
            int16_t *ptr = (int16_t*)buffer;
            
            int16_t sample_L = amplitude;
            int16_t sample_R = amplitude;

            for (int j=0;j<samples;j++)
            {
                //R-OUT
                *ptr++ = sample_R;
                //L-OUT
                *ptr++ = sample_L;
                result+=2*chn;
            }

            return result;            
        }

        void samplingtest()
        {
            // Se genera un pulso cuadrado de T = 1.00015s

            while(true)
            {
                int samples = 441;
                uint8_t buffer[samples*2*channels];

                for (int i=0;i<49;i++)
                {
                    // Escribimos el tren de pulsos en el procesador de Audio
                    m_kit.write(buffer, createTestSignal(buffer, samples, 30000));            
                }

                for (int j=0;j<49;j++)
                {
                    // Escribimos el tren de pulsos en el procesador de Audio
                    m_kit.write(buffer, createTestSignal(buffer, samples, -30000));            
                }
            }         

        }

        // Constructor
        ZXProcessor()
        {
          // Constructor de la clase
          ACU_ERROR = 0;
        }

};
