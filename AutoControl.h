#pragma once

class Controller {
public:
    virtual ~Controller() = default;

    // this line is gemini suggested so i will review it later
    virtual void reset() = 0;

    virtual double update(int setpoint, double current_measurement_value, double dt) = 0;

    virtual QVector<QPair<QString,double>> return_parameter_list() = 0;

    virtual void set_new_bounds(double lower_bound, double upper_bound) {}
    virtual void set_initial_throttle(int initial_throttle){}

    virtual void set_new_parameters(const QVector<QPair<QString, double>>& new_params) = 0;
};

class StepController : public Controller {
 public:
 
 StepController(double lower_target_bound, double upper_target_bound) {
     this->lower_target_bound = lower_target_bound;
     this->upper_target_bound = upper_target_bound;
     this->current_output_throttle = 0.0; 
     this->throttle_increase_step = 0; 
 }

 void reset() override {}

 double update(int setpoint, double current_measurement_value, double dt) override {
    
     if (current_measurement_value < lower_target_bound) {
         current_output_throttle += throttle_increase_step;
     }

     else if (current_measurement_value > upper_target_bound) {
         current_output_throttle -= throttle_increase_step;
     }

     return current_output_throttle;
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

 void set_initial_throttle(int initial_throttle) {
     this->current_output_throttle = initial_throttle;
 }

 void set_new_parameters(const QVector<QPair<QString, double>>& new_params) override {
     this->throttle_increase_step = (int)new_params[0].second;
 }

 private:
    int throttle_increase_step;
    double lower_target_bound;
    double upper_target_bound;
    double current_output_throttle;
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
    }

    double update(int setpoint, double current_measurement_value, double dt) override { // kanw override to virtual method ths base class
        if (dt <= 0) return 0.0;

        double error = (double)setpoint - current_measurement_value;
        double p_term = error * this->Kp;
        double d_term = (error - prev_error) / dt;

        i_term = anti_windup_filter(i_term, dt, error, p_term + i_term + d_term);
  
        double new_output = p_term + i_term + d_term;
        
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
     
     enum AntiWindupTechniques { IntegralClamping, ConditionalIntegration, SimpleTaring};
     AntiWindupTechniques current_aw_technique = IntegralClamping;

};

class ADRCController : public Controller {
 public:

     ADRCController(double b0_val, double wc_val, double wo_val, double min_out, double max_out) {
         this->b0 = b0_val;
         this->wc = wc_val;
         this->min_limit = min_out;
         this->max_limit = max_out;

         // Υπολογισμός των κερδών του παρατηρητή (για ζ=1) μία φορά στην αρχή
         this->beta1 = 2.0 * wo_val;
         this->beta2 = wo_val * wo_val;
     }

     double update(int setpoint, double current_measurement_value, double dt) {

         return (double)setpoint;
     }

     void reset() override {
         last_u = 0.0;
         z1 = 0.0; // Μηδενίζουμε και τη μνήμη του παρατηρητή
         z2 = 0.0;
     }

     // Το Bumpless Transfer
     void set_initial_throttle(int initial_throttle) override {
         this->last_u = initial_throttle;
     }

     void set_new_parameters(const QVector<QPair<QString, double>>& new_params) override {
         for (const auto& param : new_params) {
             if (param.first == "Bo Value") this->b0 = param.second;
             else if (param.first == "Wc Value") this->wc = param.second;
             else if (param.first == "Beta1 Value") this->beta1 = param.second;
             else if (param.first == "Beta2 Value") this->beta2 = param.second;
         }
     }

     QVector<QPair<QString, double>> return_parameter_list() {
         QVector<QPair<QString, double>> parameter_list;

         parameter_list.push_back(qMakePair("Bo Value", this->b0));
         parameter_list.push_back(qMakePair("Wc Value", this->wc));
         parameter_list.push_back(qMakePair("Beta1 Value", this->beta1));
         parameter_list.push_back(qMakePair("Beta2 Value", this->beta2));
         return parameter_list;
     }

private:
    // Η μνήμη του ADRC
    int last_u = 0; // Το προηγούμενο PWM
    double z1 = 0.0;     // Η εκτίμηση του Παρατηρητή για τις RPM
    double z2 = 0.0;     // Η εκτίμηση του Παρατηρητή για τη Διαταραχή

    // Σταθερές συστήματος - Αυτές θα γίνουν tune
    double b0;
    double wc;
    double beta1;
    double beta2;

    int min_limit;
    int max_limit;
};