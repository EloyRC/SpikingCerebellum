/*
 *  cd_poisson_generator.h
 *
 *  This file is based on the poisson generator model distributed with NEST.
 *  
 *  Modified by: Jesús Garrido (jgarridoalcazar at gmail.com) in 2017.
 */

#ifndef CD_POISSON_GENERATOR_H
#define CD_POISSON_GENERATOR_H

// Includes from librandom:
#include "poisson_randomdev.h"

// Includes from nestkernel:
#include "connection.h"
#include "event.h"
#include "nest_types.h"
#include "device_node.h"
#include "stimulating_device.h"
#include "ring_buffer.h"
#include "universal_data_logger.h"

/* BeginDocumentation
Name: cd_poisson_generator - simulate neuron firing with Poisson processes
                          statistics drive by input current.
Description:
  The cd_poisson_generator simulates a neuron that is firing with Poisson
  statistics, i.e. exponentially distributed interspike intervals. Its firing
  rate is linearly calculated based on the total amount of input current. It will
  generate a _unique_ spike train for each of it's targets. If you do not want
  this behavior and need the same spike train for all targets, you have to use a
  parrot neuron inbetween the poisson generator and the targets.

Parameters:
   The following parameters appear in the element's status dictionary:

   origin   double - Time origin for device timer in ms
   start    double - begin of device application with resp. to origin in ms
   stop     double - end of device application with resp. to origin in ms
   min_rate double - Min firing rate in Hz
   max_rate double - Max firing rate in Hz
   min_current    double - The firing rate will be min_rate when the input
                      current is below this parameter.
   max_current    double - The firing rate will be max_rate when the input
                      current is above this parameter.

Sends: SpikeEvent

Remarks:
   A Poisson generator may, especially at high rates, emit more than one
   spike during a single time step. If this happens, the generator does
   not actually send out n spikes. Instead, it emits a single spike with
   n-fold synaptic weight for the sake of efficiency.

SeeAlso: poisson_generator, Device, parrot_neuron
*/

// Define name constants for state variables and parameters
namespace nest
{
	namespace names
	{
    	// Neuron parameters
    	extern const Name min_rate;  
    	extern const Name max_rate;  
      extern const Name min_current;
      extern const Name max_current;
    }
}

namespace mynest
{
  /**
   * Function computing right-hand side of ODE for GSL solver.
   * @note Must be declared here so we can befriend it in class.
   * @note Must have C-linkage for passing to GSL. Internally, it is
   *       a first-class C++ function, but cannot be a member function
   *       because of the C-linkage.
   * @note No point in declaring it inline, since it is called
   *       through a function pointer.
   * @param void* Pointer to model neuron instance.
   */
  //extern "C"
  //int cd_poisson_generator_dynamics (double, const double*, double*, void*);
  
  class cd_poisson_generator : public nest::DeviceNode
  {
    
  public:        
    
    cd_poisson_generator();
    cd_poisson_generator(const cd_poisson_generator&);
    ~cd_poisson_generator();

  
    /**
     * Import sets of overloaded virtual functions.
     * We need to explicitly include sets of overloaded
     * virtual functions into the current scope.
     * According to the SUN C++ FAQ, this is the correct
     * way of doing things, although all other compilers
     * happily live without.
     */

    using nest::Node::handles_test_event;
    using nest::Node::handle;

    nest::port send_test_event(nest::Node&, nest::rport, nest::synindex, bool);

    void handle(nest::CurrentEvent &);
    void handle(nest::DataLoggingRequest &); 
    
    nest::port handles_test_event(nest::CurrentEvent &, nest::rport);
    nest::port handles_test_event(nest::DataLoggingRequest &, nest::rport);

    
    void get_status(DictionaryDatum &) const;
    void set_status(const DictionaryDatum &);
    
  private:
    void init_state_(const Node& proto);
    void init_buffers_();
    void calibrate();
    
    void update(nest::Time const &, const long, const long);
    
    // END Boilerplate function declarations ----------------------------

    // Friends --------------------------------------------------------
    // The next two classes need to be friends to access the State_ class/member
    friend class nest::RecordablesMap<cd_poisson_generator>;
    friend class nest::UniversalDataLogger<cd_poisson_generator>;

  private:

    /**
      * Store independent parameters of the model.
      */
    struct Parameters_{
      double min_rate_;
      double max_rate_;
      double min_current_;
      double max_current_;
      
      Parameters_(); //!< Sets default parameter values

      void get( DictionaryDatum& ) const; //!< Store current values in dictionary
      void set( const DictionaryDatum& ); //!< Set values from dicitonary
    };

  public:
    // ---------------------------------------------------------------- 

    /**
     * State variables of the model.
     * @note Copy constructor and assignment operator required because
     *       of C-style array.
     */
    struct State_ {

      double rate_;

      double input_current_; 

      State_(const Parameters_&);  //!< Default initialization
      State_(const State_&);
      
      void get(DictionaryDatum&) const;
      void set(const DictionaryDatum&, const Parameters_&);
    };    

    // ---------------------------------------------------------------- 

  private:

    /**
     * Buffers of the model.
     */
    struct Buffers_ {
      Buffers_(cd_poisson_generator&);                   //!<Sets buffer pointers to 0
      Buffers_(const Buffers_&, cd_poisson_generator&);  //!<Sets buffer pointers to 0

      //! Logger for all analog data
      nest::UniversalDataLogger<cd_poisson_generator> logger_;

      /** buffers and sums up incoming spikes/currents */
      nest::RingBuffer currents_;

      // IntergrationStep_ should be reset with the neuron on ResetNetwork,
      // but remain unchanged during calibration. Since it is initialized with
      // step_, and the resolution cannot change after nodes have been created,
      // it is safe to place both here.
      double step_;           //!< step size in ms
    };

  // ------------------------------------------------------------

    struct Variables_
    {
      librandom::PoissonRandomDev poisson_dev_; //!< Random deviate generator
    };

    // Access functions for UniversalDataLogger -------------------------------
    
    //! Read out state vector elements, used by UniversalDataLogger
    double get_rate_() const { return S_.rate_; }

    //! Read out state vector elements, used by UniversalDataLogger
    double get_current_() const { return S_.input_current_; }

  // ------------------------------------------------------------

    Parameters_ P_;
    Variables_ V_;
    State_ S_;
    Buffers_ B_;

    //! Mapping of recordables names to access functions
    static nest::RecordablesMap<cd_poisson_generator> recordablesMap_;

  };

  
  inline
  nest::port cd_poisson_generator::send_test_event(nest::Node& target, nest::rport receptor_type, nest::synindex syn_id, bool dummy_target)
  {
  	nest::SpikeEvent e;
    e.set_sender( *this );
    return target.handles_test_event( e, receptor_type );
  }

  inline
  nest::port cd_poisson_generator::handles_test_event(nest::CurrentEvent&, nest::rport receptor_type)
  {
    if (receptor_type != 0)
      throw nest::UnknownReceptorType(receptor_type, get_name());
    return 0;
  }

  inline
  nest::port cd_poisson_generator::handles_test_event(nest::DataLoggingRequest& dlr, nest::rport receptor_type)
  {
    if (receptor_type != 0)
      throw nest::UnknownReceptorType(receptor_type, get_name());
    return B_.logger_.connect_logging_device(dlr, recordablesMap_);
  }

  inline
  void cd_poisson_generator::get_status(DictionaryDatum &d) const
  {
    P_.get(d);
    S_.get(d);
  }

  inline
  void cd_poisson_generator::set_status(const DictionaryDatum &d)
  {
    Parameters_ ptmp = P_;  // temporary copy in case of errors
    ptmp.set(d);                       // throws if BadProperty
    State_      stmp = S_;  // temporary copy in case of errors
    stmp.set(d, ptmp); 
    
    // if we get here, temporaries contain consistent set of properties
    P_ = ptmp;
    S_ = stmp;
    
  }
  
} // namespace

#endif //cd_poisson_generator_H
