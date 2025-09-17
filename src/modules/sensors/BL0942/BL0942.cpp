
#include "Global.h"
#include "classes/IoTUart.h"
#include "datatypes.h"

//namespace bl0942
//{
    class BL0942cmd;
    BL0942cmd *BL0942 = nullptr;
    DataPacket buffer;
    uint8_t inpos = 0xFF;
    uint8_t checksum;
    uint8_t pubPhase = 0xFF;
    bool needUpdate = false;

    static const float BL0942_PREF = 596;             // taken from tasmota
    static const float BL0942_UREF = 15873.35944299;  // should be 73989/1.218
    static const float BL0942_IREF = 251213.46469622; // 305978/1.218
    static const float BL0942_EREF = 3304.61127328;   // Measured

    static const char *const TAG = "bl0942";

    static const uint8_t BL0942_READ_COMMAND = 0x58;
    static const uint8_t BL0942_FULL_PACKET = 0xAA;
    static const uint8_t BL0942_PACKET_HEADER = 0x55;

    static const uint8_t BL0942_WRITE_COMMAND = 0xA8;
    static const uint8_t BL0942_REG_I_FAST_RMS_CTRL = 0x10;
    static const uint8_t BL0942_REG_MODE = 0x18;
    static const uint8_t BL0942_REG_SOFT_RESET = 0x19;
    static const uint8_t BL0942_REG_USR_WRPROT = 0x1A;
    static const uint8_t BL0942_REG_TPS_CTRL = 0x1B;

    // TODO: Confirm insialisation works as intended
    const uint8_t BL0942_INIT[5][6] = {
        // Reset to default
        {BL0942_WRITE_COMMAND, BL0942_REG_SOFT_RESET, 0x5A, 0x5A, 0x5A, 0x38},
        // Enable User Operation Write
        {BL0942_WRITE_COMMAND, BL0942_REG_USR_WRPROT, 0x55, 0x00, 0x00, 0xF0},
        // 0x0100 = CF_UNABLE energy pulse, AC_FREQ_SEL 50Hz, RMS_UPDATE_SEL 800mS
        {BL0942_WRITE_COMMAND, BL0942_REG_MODE, 0x00, 0x10, 0x00, 0x37},
        // 0x47FF = Over-current and leakage alarm on, Automatic temperature measurement, Interval 100mS
        {BL0942_WRITE_COMMAND, BL0942_REG_TPS_CTRL, 0xFF, 0x47, 0x00, 0xFE},
        // 0x181C = Half cycle, Fast RMS threshold 6172
        {BL0942_WRITE_COMMAND, BL0942_REG_I_FAST_RMS_CTRL, 0x1C, 0x18, 0x00, 0x1B}};


    class BL0942cmd : public IoTUart
    {
    private:
        float i_rms, watt, v_rms, frequency, total_energy_consumption = 0;
        // Divide by this to turn into Watt
        float power_reference_ = BL0942_PREF;
        // Divide by this to turn into Volt
        float voltage_reference_ = BL0942_UREF;
        // Divide by this to turn into Ampere
        float current_reference_ = BL0942_IREF;
        // Divide by this to turn into kWh
        float energy_reference_ = BL0942_EREF;

    public:
        BL0942cmd(String parameters) : IoTUart(parameters)
        {
            /*
            jsonRead(parameters, "R_current", CURRENT_RESISTOR);
            jsonRead(parameters, "R_upstream", VOLTAGE_RESISTOR_UPSTREAM);
            jsonRead(parameters, "R_downstream", VOLTAGE_RESISTOR_DOWNSTREAM);
            jsonRead(parameters, "CF_GPIO", BL0942_CF_GPIO);
            jsonRead(parameters, "CF1_GPIO", BL0942_CF1_GPIO);
            jsonRead(parameters, "SEL_GPIO", BL0942_SEL_GPIO_INV);
            jsonRead(parameters, "kfV", _kfV);
            jsonRead(parameters, "kfA", _kfA);
            jsonRead(parameters, "kfW", _kfW);
    */
            for (auto *i : BL0942_INIT)
            {
                _myUART->write(i, 6);
                delay(1);
            }
            _myUART->flush();
            BL0942 = this;
        }

        void loop()
        {
            if (_myUART->available())
            {
                while (_myUART->available())
                {
                    uint8_t in;
                    _myUART->readBytes(&in, 1);
                    if (inpos < sizeof(buffer) - 1)
                    { // читаем тело пакета
                        ((uint8_t *)(&buffer))[inpos] = in;
                        inpos++;
                        checksum += in;
                    }
                    else if (inpos < sizeof(buffer))
                    { // получили контрольную сумму
                        inpos++;
                        checksum ^= 0xFF;
                        if (in != checksum)
                        {
                            //ESP_LOGE(TAG, "BL0942 invalid checksum! 0x%02X != 0x%02X", checksum, in);
                            SerialPrint("E", "BL0942cmd", "Invalid checksum!", _id);
                        }
                        else
                        {
                            pubPhase = 0;
                        }
                    }
                    else
                    {
                        if (in == BL0942_PACKET_HEADER)
                        { // стартовый хидер
                            ((uint8_t *)(&buffer))[0] = BL0942_PACKET_HEADER;
                            inpos = 1;                                             // начало сохранения буфера
                            checksum = BL0942_READ_COMMAND + BL0942_PACKET_HEADER; // начальные данные рассчета кс
                            pubPhase = 3;
                        }
                        else
                        {
                            //ESP_LOGE(TAG, "Invalid data. Header mismatch: %d", in);
                            SerialPrint("E", "BL0942cmd", "Invalid data. Header mismatch", _id);
                        }
                    }
                }
            }
            else if (pubPhase < 3)
            {
                if (pubPhase == 0)
                {

                    i_rms = (uint24_t)buffer.i_rms / current_reference_;

                    watt = (int24_t)buffer.watt / power_reference_;

                    pubPhase = 1;
                }
                else if (pubPhase == 1)
                {

                    v_rms = (uint24_t)buffer.v_rms / voltage_reference_;

                    frequency = 1000000.0f / buffer.frequency;

                    pubPhase = 2;
                }
                else if (pubPhase == 2)
                {

                    uint32_t cf_cnt = (uint24_t)buffer.cf_cnt;
                    total_energy_consumption = cf_cnt / energy_reference_;

                    pubPhase = 3;
                }
            }
            IoTItem::loop();
        }
        void doByInterval()
        {
            _myUART->write(BL0942_READ_COMMAND);
            _myUART->write(BL0942_FULL_PACKET);
        }
        float getEnergy() { return total_energy_consumption; }
        float getPower() { return watt; }
        float getCurrent() { return i_rms; }
        float getVoltage() { return v_rms; }

        ~BL0942cmd(){

        };
    };

    class BL0942v : public IoTItem
    {
    private:
    public:
        BL0942v(String parameters) : IoTItem(parameters)
        {
        }

        void doByInterval()
        {
            if (BL0942)
                regEvent(BL0942->getVoltage(), "BL0942 V");
            else
            {
                regEvent(NAN, "BL0942v");
                SerialPrint("E", "BL0942cmd", "initialization error", _id);
            }
        }

        ~BL0942v(){};
    };

    class BL0942a : public IoTItem
    {
    private:
    public:
        BL0942a(String parameters) : IoTItem(parameters)
        {
        }

        void doByInterval()
        {
            if (BL0942)
                regEvent(BL0942->getCurrent(), "BL0942 A");
            else
            {
                regEvent(NAN, "BL0942a");
                SerialPrint("E", "BL0942cmd", "initialization error", _id);
            }
        }

        ~BL0942a(){};
    };

    class BL0942w : public IoTItem
    {
    private:
    public:
        BL0942w(String parameters) : IoTItem(parameters)
        {
        }

        void doByInterval()
        {
            if (BL0942)
                regEvent(BL0942->getPower(), "BL0942 W");
            else
            {
                regEvent(NAN, "BL0942w");
                SerialPrint("E", "BL0942cmd", "initialization error", _id);
            }
        }

        ~BL0942w(){};
    };

    class BL0942wh : public IoTItem
    {
    private:
    public:
        BL0942wh(String parameters) : IoTItem(parameters)
        {
        }

        void doByInterval()
        {
            if (BL0942)
                regEvent(BL0942->getEnergy() / 3600.0 / 1000.0, "BL0942 Wh");
            else
            {
                regEvent(NAN, "BL0942wh");
                SerialPrint("E", "BL0942cmd", "initialization error", _id);
            }
        }

        ~BL0942wh(){};
    };

//} // namespace bl0942

void *getAPI_BL0942(String subtype, String param)
{
    if (subtype == F("BL0942v"))
    {
        return new BL0942v(param);
    }
    else if (subtype == F("BL0942a"))
    {
        return new BL0942a(param);
    }
    else if (subtype == F("BL0942w"))
    {
        return new BL0942w(param);
    }
    else if (subtype == F("BL0942wh"))
    {
        return new BL0942wh(param);
    }
    else if (subtype == F("BL0942cmd"))
    {
        return new BL0942cmd(param);
    }
    else
    {
        return nullptr;
    }
}
