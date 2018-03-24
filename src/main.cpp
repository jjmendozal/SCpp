#include "model_landing_6dof.h"

#include <iostream>
#include <array>
#include <cmath>
#include <ctime>

#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/eigen/eigen_algebra.hpp>

using namespace std;
using namespace boost::numeric::odeint;


using Model = model_landing_6dof;
model_landing_6dof model;


const int iterations = 15;

typedef Eigen::Matrix<double,14,23> state_type;

class ode_dVdt{
private:
    Model::ControlVector u_t, u_t1;
    double sigma, dt;

public:
    void Update(const Model::ControlVector &u_t, const Model::ControlVector &u_t1, const double &sigma, double dt){
        this->dt = dt;
        this->u_t = u_t;
        this->u_t1 = u_t1;
        this->sigma = sigma;
    }

    void operator()(const state_type &V, state_type &dVdt, const double t){

        const Model::StateVector &x = V.col(0);
        const Model::ControlVector u = u_t + t / dt * (u_t1 - u_t);

        const double alpha = t / dt;
        const double beta = 1. - alpha;

        const Model::StateMatrix   A_bar  = sigma * model.state_jacobian(x, u);
        const Model::ControlMatrix B_bar  = sigma * model.control_jacobian(x, u);
        const Model::StateVector   f      =         model.ode(x, u);


        Model::StateMatrix Phi_A_xi = V.block<Model::n_states, Model::n_states>(0, 1);
        Model::StateMatrix Phi_A_xi_inverse = Phi_A_xi.inverse();

        size_t cols = 0;

        dVdt.block<Model::n_states, 1>(0, cols) = sigma * f;
        cols += 1;

        dVdt.block<Model::n_states, Model::n_states>(0, cols) = A_bar * Phi_A_xi;
        cols += Model::n_states;

        dVdt.block<Model::n_states, Model::n_inputs>(0, cols) = Phi_A_xi_inverse * B_bar * alpha;
        cols += Model::n_inputs;
        
        dVdt.block<Model::n_states, Model::n_inputs>(0, cols) = Phi_A_xi_inverse * B_bar * beta;
        cols += Model::n_inputs;
        
        dVdt.block<Model::n_states, 1>(0, cols) = Phi_A_xi_inverse * f;
        cols += 1;
        
        dVdt.block<Model::n_states, 1>(0, cols) = Phi_A_xi_inverse * (-A_bar * x - B_bar * u);

        

    }
};

int main() {

    //trajectory points
    constexpr int K = 50;
    const double dt = 1 / double(K-1);

    MatrixXd X(Model::n_states, K);
    MatrixXd U(3, K);


    // START INITIALIZATION
    cout << "Starting initialization." << endl;
    model.initialize(K, X, U);
    cout << "Initialization finished." << endl;

    // START SUCCESSIVE CONVEXIFICATION
    
    double sigma = model.total_time_guess();

    state_type V;

    array<Model::StateMatrix,   K> A_bar;
    array<Model::ControlMatrix, K> B_bar;
    array<Model::ControlMatrix, K> C_bar;
    array<Model::StateVector,   K> Sigma_bar;
    array<Model::StateVector,   K> z_bar;

    runge_kutta_dopri5<state_type, double, state_type, double, vector_space_algebra> stepper;
    ode_dVdt dVdt;

    for(int it = 1; it < iterations + 1; it++) {
        cout << "Iteration " << it << endl;
        cout << "Calculating new transition matrices." << endl;

        const clock_t begin_time = clock();

        for (int k = 0; k < K-1; k++) {
            V.setZero();
            V.col(0) = X.col(k);
            V.block<Model::n_states,Model::n_states>(0, 1).setIdentity();

            dVdt.Update(U.col(k), U.col(k+1), sigma, dt);
            integrate_adaptive(make_controlled(1E-12 , 1E-12 , stepper), dVdt, V, 0., dt, dt/10.);

            A_bar[k] = V.block<14,14>(0, 1);
            B_bar[k] = V.block<14,14>(0, 1) * V.block<14,3>(0, 15);
            C_bar[k] = V.block<14,14>(0, 1) * V.block<14,3>(0, 18);
            Sigma_bar[k] = V.block<14,14>(0, 1) * V.block<14,1>(0, 21);
            z_bar[k] = V.block<14,14>(0, 1) * V.block<14,1>(0, 22);

            // debug print for refactoring, remove later
            cout << A_bar[k] << endl;
            cout << B_bar[k] << endl;
            cout << C_bar[k] << endl;
            cout << Sigma_bar[k] << endl;
            cout << z_bar[k] << endl;
        }
        //cout << "Transition matrices calculated in " << double( clock () - begin_time ) /  CLOCKS_PER_SEC << " seconds." << endl;

        // TODO: Solve problem.


    }

}