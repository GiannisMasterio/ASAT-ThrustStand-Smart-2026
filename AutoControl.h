#pragma once
#include "VarState.h"

class Controller {
public:
    virtual ~Controller() = default;

    // this line is gemini suggested so i will review it later
    virtual void reset() = 0;

    virtual int update(double setpoint, double current_measurement_value, int current_throttle, double dt) = 0;

    virtual QVector<QPair<QString,double>> return_parameter_list() = 0;

    virtual void set_new_bounds(double lower_bound, double upper_bound) {}
    virtual void set_initial_throttle(int initial_throttle){}

    virtual void set_new_parameters(const QVector<QPair<QString, double>>& new_params) = 0;
};

class StepController : public Controller {

 public:
 
 StepController(double lower_target_bound, double upper_target_bound, int throttle_increase_step) {
     this->lower_target_bound = lower_target_bound;
     this->upper_target_bound = upper_target_bound;
     this->throttle_increase_step = throttle_increase_step;
 }

 void reset() override {}

 int update(double setpoint, double current_measurement_value, int current_throttle, double dt) override {
    
     if (current_measurement_value < lower_target_bound) {
         current_throttle += throttle_increase_step;
     }

     else if (current_measurement_value > upper_target_bound) {
         current_throttle -= throttle_increase_step;
     }

     return current_throttle;
 }

 QVector<QPair<QString, double>> return_parameter_list() {
     QVector<QPair<QString, double>> parameter_list;

     parameter_list.push_back(qMakePair("Throttle Increase Step", (double)(this->throttle_increase_step)));
     return parameter_list;
 }

 void set_new_bounds(double lower_target_bound, double upper_target_bound) override{
     this->lower_target_bound = lower_target_bound;
     this->upper_target_bound = upper_target_bound;
 }

 void set_new_parameters(const QVector<QPair<QString, double>>& new_params) override {
     this->throttle_increase_step = (int)new_params[0].second;
 }

 private:
    int throttle_increase_step;
    double lower_target_bound;
    double upper_target_bound;
};

class PidController : public Controller {
 public:
    
    PidController(double Kp, double Ki, double Kd, double i_term, int min_real_limit, int max_real_limit) {
        this->Kp = Kp;
        this->Ki = Ki;
        this->Kd = Kd;
        this->i_term = i_term;
        setOutputLimits(min_real_limit, max_real_limit);
    }

    void setOutputLimits(int min_val, int max_val) {
        this->min_real_limit = min_val;
        this->max_real_limit = max_val;
    }

    void reset() override {
        this->i_term = 0;
        this->prev_error = 0;
        is_initialized = false;
    }

    int update(double setpoint, double current_measurement_value, int current_throttle, double dt) override { // kanw override to virtual method ths base class
        if (dt <= 0) return 0.0;

        if (!is_initialized) {
            this->i_term = current_throttle; // αυτοματη συσσωρευση στον ολοκληρωτικο ορο
            this->prev_error = setpoint - current_measurement_value;
			is_initialized = true;
        }

        double error = (double)setpoint - current_measurement_value;
        double p_term = error * this->Kp;
        double d_term = this->Kd * (error - prev_error) / dt;

        i_term = anti_windup_filter(i_term, dt, error, p_term + i_term + d_term);
  
        int new_output = p_term + i_term + d_term;
        
        this->prev_error = error;

        return new_output;
    }

    void set_clamping_values() {
        int interval_range = max_real_limit - min_real_limit;

        this->i_max = clamping_authority * interval_range;
        this->i_min = -1 * this->i_max; 
    }

    void set_initial_throttle(int initial_throttle) override {
        this->i_term = initial_throttle;
    }

    QVector<QPair<QString, double>> return_parameter_list() {
        QVector<QPair<QString, double>> parameter_list;

        parameter_list.push_back(qMakePair("Kp Value", this->Kp));
        parameter_list.push_back(qMakePair("Ki Value", this->Ki));
        parameter_list.push_back(qMakePair("Kd Value", this->Kd));
        parameter_list.push_back(qMakePair("Clamping Authority (For Clamping Anti-Windup)", this->clamping_authority));
        parameter_list.push_back(qMakePair("AntiWindup Technique", this->current_aw_technique));
        return parameter_list;
    }

    double anti_windup_filter(double i_term, double dt, double error, double output_unsaturated) {

        switch (this->current_aw_technique) {

        case IntegralClamping:

            i_term += this->Ki * error * dt;

            set_clamping_values();

            if (i_term > this->i_max)
                i_term = this->i_max;

            if (i_term < this->i_min)
                i_term = this->i_min;

            break;

        case ConditionalIntegration:
        {
            
            bool saturated_high = output_unsaturated > max_real_limit;
            bool saturated_low = output_unsaturated < min_real_limit;

            // Δεν είμαστε saturated
            if (!saturated_high && !saturated_low) {

                i_term += this->Ki * error * dt;
            }

            // Saturated high: integration επιτρέπεται μόνο αν μειώνει το output
            else if (saturated_high && error < 0) {

                i_term += this->Ki * error * dt;
            }

            // Saturated low: integration επιτρέπεται μόνο αν αυξάνει το output
            else if (saturated_low && error > 0) {

                i_term += this->Ki * error * dt;
            }

            break;
        }
        case SimpleTaring:


            break;
        }

        return i_term;
    }

    void set_new_parameters(const QVector<QPair<QString, double>>& new_params) override {
        for (const auto& param : new_params) {
            if (param.first == "Kp Value") this->Kp = param.second;
            else if (param.first == "Ki Value") this->Ki = param.second;
            else if (param.first == "Kd Value") this->Kd = param.second;
            else if (param.first == "Clamping Authority (For Clamping Anti-Windup)") this->clamping_authority = param.second;
            else if (param.first == "AntiWindup Technique") this->current_aw_technique = static_cast<AntiWindupTechniques>((int)param.second);
        }
    }

 private:
     int min_real_limit;
     int max_real_limit;

     double Kp = 0;
     double Kd = 0;
     double Ki = 0;
     double clamping_authority = 0.3; // με βαση αυτο βρισκω τα i_min και i_max

     double i_min = -100;
     double i_max = -100;
     double i_term = 0;
     double prev_error = 0;
     
     bool is_initialized = false; // αυτο το βαζω για bumpless transfer

     enum AntiWindupTechniques { IntegralClamping, ConditionalIntegration, SimpleTaring};
     AntiWindupTechniques current_aw_technique = IntegralClamping;

};

class ADRCController : public Controller {
 public:

     ADRCController(double b0_val, double wc_val, double wo_val, double min_out, double max_out) {

         this->b0 = b0_val;
         this->wc = wc_val;
         this->wo = wo_val;

         this->min_limit = min_out;
         this->max_limit = max_out;

         update_observer_gains();
     }

     int update(double setpoint, double current_measurement_value, int current_throttle, double dt) {
         if (dt <= 0.0 || std::abs(this->b0) < 1e-9) {
             return this->last_output;
         }

         if (!is_initialized) {

             estimate1 = current_measurement_value; // pairnw atofia th metrhsh arxika
			 estimate2 = -this->b0 * (double)(this->last_output);// arxikopoiw to estimate2 sto 0
             is_initialized = true;
         }

         double observer_error = estimate1 - current_measurement_value;
         double d_est1 = estimate2 + this->b0 * this->last_output - (this->beta1 * observer_error);
         double d_est2 = -(this->beta2 * observer_error);

		 estimate1 = d_est1 * dt;
		 estimate2 = d_est2 * dt;

         double u0 = this->wc * (setpoint - estimate1);
         double u = (u0 - estimate2) / this->b0; // Disturbance Rejection
         
         int new_output = std::clamp(qRound(u), this->min_limit, this->max_limit);
	     this->last_output = new_output;
         return new_output;
     }

     void update_observer_gains() {
         this->beta1 = 2.0 * this->wo;
         this->beta2 = this->wo * this->wo;
     }

     void reset() override {
         last_output = 0;
         estimate1 = 0.0; 
         estimate2 = 0.0;
         is_initialized = false;
     }

     void set_new_parameters(const QVector<QPair<QString, double>>& new_params) override {
         for (const auto& param : new_params) {
             if (param.first == "b0 (System Gain)") this->b0 = param.second;
             else if (param.first == "Wc (Controller Bandwidth)") this->wc = param.second;
             else if (param.first == "Wo (Observer Bandwidth)") this->wo = param.second;
         }
     }

     QVector<QPair<QString, double>> return_parameter_list() {
         QVector<QPair<QString, double>> parameter_list;

         parameter_list.push_back(qMakePair("b0 (System Gain)", this->b0));
         parameter_list.push_back(qMakePair("Wc (Controller Bandwidth)", this->wc));
         parameter_list.push_back(qMakePair("Wo (Observer Bandwidth)", this->wo));

         return parameter_list;
     }

private:

    // ADRC MEMORY
    int last_output = 0; // Το προηγούμενο PWM (σε double για ακρίβεια στον παρατηρητή)
	double estimate1 = 0.0;     // Observer Estimate for Measurement
    double estimate2 = 0.0;     // Observer Estimate for Total Perturbation
    bool observer_initialized = false;

    // System Constants -> We tune these
    double b0 = 10.0;
    double wc = 5.0;
    double wo = 20.0;

    // Inner Observer Gains -> Αυτα θα γινουνε update με βαση τα constants
    double beta1 = 40.0;
    double beta2 = 400.0;

    int min_limit = MIN_THROTTLE;
    int max_limit = MAX_THROTTLE;

    bool is_initialized = false; // αυτο το βαζω για bumpless transfer
};