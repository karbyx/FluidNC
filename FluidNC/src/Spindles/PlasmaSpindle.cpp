#include "PlasmaSpindle.h"

#include "../System.h"  // sys.abort

/*




*/

namespace Spindles {

    void PlasmaSpindle::init() {
        if (_output_pin.undefined()) {
            log_error("Output pin pin must be defined for Plasma Spindle");
            return;
        }

        _enable_pin.setAttr(Pin::Attr::Output);
        _output_pin.setAttr(Pin::Attr::Output);
        _arc_ok_pin.setAttr(Pin::Attr::Input);

        if (_speeds.size() == 0) {
            // The default speed map for an On/Off spindle is off - 0% -
            // for speed 0 and on - 100% - for any nonzero speedl
            // In other words there is a step transition right at 0.
            linearSpeeds(1, 100.0f);
        }
        setupSpeeds(1);
        init_atc();
        config_message();
    }

    // prints the startup message of the spindle config
    void PlasmaSpindle ::config_message() {
        log_info(name() << " Ena:" << _enable_pin.name() << " Out:" << _output_pin.name() << " Arc OK:" << _arc_ok_pin.name() << atc_info());
    }

    void PlasmaSpindle::setState(SpindleState state, SpindleSpeed speed) {

        if (sys.abort) {
            return;  // Block during abort.
        }

        // We always use mapSpeed() with the unmodified input speed so it sets
        // sys.spindle_speed correctly.
        uint32_t dev_speed = speed;                              // no mapping
        if (state == SpindleState::Disable || dev_speed == 0) {  // Halt or set spindle direction and speed.
            set_output(false);
            set_enable(false);
        } else {
            // maybe check arc OK is not on before starting

            set_output(speed);
            set_enable(true);            
        }
    }

    bool IRAM_ATTR PlasmaSpindle::wait_for_arc_ok() {
        uint32_t wait_until_ms = millis() + _max_arc_wait;
        while (millis() < wait_until_ms) {
            if (_arc_ok_pin.read()) {
                return true;
            }
            protocol_execute_realtime();
            delay_ms(1);
        }
        gc_state.modal.spindle = SpindleState::Disable;
        mc_critical(ExecAlarm::SpindleControl);
        log_error(name() << " failed to get arc OK signal");
        return false;  // failed to get arc_ok
    }

    void IRAM_ATTR PlasmaSpindle::set_output(uint32_t dev_speed) {
        log_info("set_output" << dev_speed);
        if (dev_speed) {
            if (!wait_for_arc_ok()) {
                set_output(false);
                return;
            }
        }
        _output_pin.synchronousWrite(dev_speed != 0);
    }

    void IRAM_ATTR PlasmaSpindle::setSpeedfromISR(uint32_t dev_speed) {
        set_output(dev_speed != 0);
    }

    void IRAM_ATTR PlasmaSpindle::set_enable(bool enable) {
        if (_disable_with_zero_speed && sys.spindle_speed == 0) {
            enable = false;
        }

        _enable_pin.synchronousWrite(enable);
    }

    void PlasmaSpindle::set_direction(bool Clockwise) {}

    void PlasmaSpindle::deinit() {
        stop();
        _enable_pin.setAttr(Pin::Attr::Input);
        _output_pin.setAttr(Pin::Attr::Input);
        _arc_ok_pin.setAttr(Pin::Attr::Input);
    }

    // Configuration registration
    namespace {
        SpindleFactory::InstanceBuilder<PlasmaSpindle> registration("PlasmaSpindle");
    }
}
