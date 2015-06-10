#include "simple_transport_header.h"

int main(int argc, char *argv[])
{
  int i_b;            // index over batches
  int i_a=-1;         // index over active batches
  int i_g;            // index over generations
  unsigned long i_p;  // index over particles
  FILE *fp = NULL;    // file pointer for output
  double t1, t2;      // timers
  double *keff;       // effective multiplication factor
  double keff_batch;  // keff of batch
  double keff_mean;   // keff mean over active batches
  double keff_std;    // keff standard deviation over active batches
  double H;           // shannon entropy
  Parameters *params; // user defined parameters
  Geometry *g;
  Material *m;
  Tally *t;
  Bank *source_bank;
  Bank *fission_bank;

  // Get inputs
  params = set_default_params();
  parse_params("parameters", params);
  print_params(params);

  // Set up output files
  init_output(params, fp);

  // Seed RNG
  srand(params->seed);

  // Set up array for keff
  keff = calloc(params->n_active, sizeof(double));

  // Set up geometry
  g = init_geometry(params);

  // Set up material
  m = init_material(params);

  // Set up tallies
  t = init_tally(params);

  // Initialize source bank
  source_bank = init_bank(params->n_particles);

  // Initialize fission bank
  fission_bank = init_bank(params->n_particles);

  // Sample source particles
  for(i_p=0; i_p<params->n_particles; i_p++){
    sample_source_particle(&(source_bank->p[i_p]), g);
    source_bank->n++;
  }

  center_print("SIMULATION", 79);
  border_print();
  printf("%-15s %-15s %-15s %-15s\n", "BATCH", "ENTROPY", "KEFF", "MEAN KEFF");

  // Start time
  t1 = timer();

  // Loop over batches
  for(i_b=0; i_b<params->n_batches; i_b++){

    keff_batch = 0;

    // Turn on tallying and increment index in active batches
    if(i_b >= params->n_batches - params->n_active){
      i_a++;
      if(params->tally == TRUE){
        t->tallies_on = TRUE;
      }
    }

    // Loop over generations
    for(i_g=0; i_g<params->n_generations; i_g++){

      // Loop over particles
      for(i_p=0; i_p<source_bank->n; i_p++){

        // Transport the next particle from source bank
        transport(&(source_bank->p[i_p]), g, m, t, fission_bank);
      }

      // Accumulate generation k_effective
      keff_batch += (double) fission_bank->n / source_bank->n;

      // Sample new source particles from the particles that were added to the
      // fission bank during this generation
      synchronize_bank(source_bank, fission_bank, g);

      // Calculate shannon entropy to assess source convergence
      H = shannon_entropy(g, source_bank, params);
      if(params->write_entropy == TRUE){
        write_entropy(H, fp, params->entropy_file);
      }
    }

    // Calculate k_effective
    keff_batch /= params->n_generations;
    if(i_a >= 0){
      keff[i_a] = keff_batch;
    }

    // Tallies for this realization
    if(t->tallies_on == TRUE){
      batch_tally(t, params);
      if(params->write_tally == TRUE){
        write_tally(t, fp, params->tally_file);
      }
    }

    // Calculate keff mean and standard deviation
    calculate_keff(keff, &keff_mean, &keff_std, i_a+1);

    // Status text
    if(i_a < 0){
      printf("%-15d %-15f %-15f\n", i_b+1, H, keff_batch);
    }
    else{
      printf("%-15d %-15f %-15f %f +/- %-15f\n", i_b+1, H, keff_batch, keff_mean, keff_std);
    }
  }

  // Write out keff
  if(params->write_keff == TRUE){
    write_keff(keff, params->n_active, fp, params->keff_file);
  }

  // Stop time
  t2 = timer();

  printf("Simulation time: %f secs\n", t2-t1);

  // Free memory
  free_bank(fission_bank);
  free_bank(source_bank);
  free_tally(t);
  free_material(m);
  free(g);
  free(keff);
  free(params);

  return 0;
}