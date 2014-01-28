#include "blobs_tracker.hpp"

#include "blobs_finder.hpp"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <vector>

using namespace cv;
using namespace std;

/*
bool operator< (Subnode const &a, Subnode const &b) {
	return ( a.badness < b.badness );
}
*/

void BlobsTracker::InsertFrameInTimeline(vector<Blob> blobs, int time) {
	vector<Node> v;
	for (int i=0; i<blobs.size(); i++) {
		Node n = Node( blobs[i], time, false );
		v.push_back(n);
	}
	
	// Inserting the absent-ball node
	Blob phantom ( Point2f(0.0,0.0), 0.0, 0.0, 0.0 );
	Node n ( phantom, time, true );
	v.push_back(n);
	
	_timeline.push_back(v);
}


vector<Point2f> BlobsTracker::ProcessFrames(int initial_time, int begin_time, int end_time) {
	logger(panel, "ball tracking", INFO) << "Processing frames from " << begin_time << " to " << end_time-1 << endl;
	
	double min_badness = INFTY;
	Node *best_node = NULL;
	
	for (int i=0; i<_timeline.size(); i++) {
		for (int k=0; k<_timeline[i].size(); k++) {
			
			Node &old_node = _timeline[i][k];
			
			// Passo base (collegamenti con il nodo fittizio iniziale)
			if ( old_node.is_absent )
				old_node.badness = i == 0 ? _disappearance_parameter : INFTY;
			else
				old_node.badness = i * _skip_parameter;
			old_node.previous = NULL;
			
			// Programmazione dinamica
			for (int j=0; j<i; j++) {
				for (int h=0; h<_timeline[j].size(); h++) {
					
					Node &new_node = _timeline[j][h];
					
					double old_badness = new_node.badness;
					int interval = i-j;
					
					if ( (old_node.is_absent || new_node.is_absent) && interval > 1) {
						// I nodi assenti non skippano frames
						continue;
					}

					double delta_badness = (double)(interval-1) * _skip_parameter;
					
					if ( old_node.is_absent && new_node.is_absent ) {
						// Transizione da assente ad assente
						delta_badness += _absent_parameter;
					}
					
					if ( old_node.is_absent && !new_node.is_absent ) {
						// Transizione da assente a presente
						delta_badness += _appearance_parameter - new_node.blob.weight;
					}
					
					if ( !old_node.is_absent && new_node.is_absent ) {
						// Transizione da presente ad assente
						delta_badness += _disappearance_parameter;
					}
					
					
					if ( !old_node.is_absent && !new_node.is_absent ) {
						// Transizione da presente a presente
						
						delta_badness += - new_node.blob.weight;
						logger(panel, "ball tracking", DEBUG) << "WEIGHT: " << new_node.blob.weight << endl;
					
						// Località spaziale
						Point2f new_center = old_node.blob.center;
						Point2f old_center = new_node.blob.center;
						double distance = norm(new_center - old_center);
					
						// Controllo di località
						if ( distance > _max_speed / _fps * interval ) continue;
						if ( distance > _max_unseen_distance ) continue;
					
						// Verosimiglianza gaussiana
						double time_diff = interval / _fps;
						delta_badness += distance * distance / ( time_diff * _variance_parameter );

						logger(panel, "ball tracking", DEBUG) << "DELTA BADNESS: " << delta_badness << endl;
					}
					
					// Calcolo la nuova badness, e aggiorno se è minore della minima finora trovata
					double new_badness = old_badness + delta_badness;
					
					if ( new_badness < old_node.badness ) {
						old_node.badness = new_badness;
						// Salvo il percorso ottimo
						old_node.previous = &new_node;
					}
					
				}
			}

			int time_to_end = _timeline.size() - i - 1;

			// Passo finale (collegamenti con il nodo fittizio finale)
			double final_cost;
			if ( old_node.is_absent )
				final_cost = time_to_end == 0 ? _appearance_parameter : INFTY;
			else
				final_cost = time_to_end * _skip_parameter;
			double candidate_badness = _timeline[i][k].badness + final_cost;
			
			if ( min_badness > candidate_badness ) {
				min_badness = candidate_badness;
				best_node = &_timeline[i][k];
			}
			
		}
	}
	
	Node *node = best_node;
	while ( node != NULL && node->previous != NULL ) {
		node->previous->next = node;
		node = node->previous;
	}
	
	
	Point2f NISBA(1000.0,1000.0);
	vector<Point2f> positions;
	Point2f position;
	
	logger(panel, "ball tracking", VERBOSE) << "Badness: " << min_badness << endl;
	
	for (int i=begin_time; i<end_time; i++) {
		
		// Cerco la miglior posizione prevista per il frame i
		Node *node = best_node;
		if ( node == NULL ) {
			logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
					"no ball found (no path)" << endl;
			positions.push_back(NISBA);
			continue;
		}
		if ( node->time < i ) {
			logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
					"no ball found (path does not pass through current frame)" << endl;
			positions.push_back(NISBA);
			continue;
		}
	
		Node *greater = NULL;
		Node *lower = NULL;
	
		while ( node != NULL ) {
			int time = node->time;
			if ( time >= i ) {
				greater = node;
			}
			if ( time <= i ) {
				lower = node;
				break;
			}
			node = node->previous;
		}
		
		// Smoothing
		
		Node* n;
		
		Point2f smoothed (0.0, 0.0);
		double total_weight = 0.0;
		
		double sigma = 0.02 * _fps;
		
		n = greater;
		if ( lower == greater ) n = n->next;
		for (int k=0; k<3; ++k) {
			if ( n == NULL || n->is_absent ) break;
			
			double d = i - n->time;
			double w = exp( - abs(d) / (2*sigma*sigma) );
			
			smoothed += n->blob.center * w;
			total_weight += w;
			
			n = n->next;
		}
		
		n = lower;
		if ( lower == greater ) n = n->previous;
		for (int k=0; k<3; ++k) {
			if ( n == NULL || n->is_absent ) break;
			
			double d = i - n->time;
			double w = exp( - abs(d) / (2*sigma*sigma) );
			
			smoothed += n->blob.center * w;
			total_weight += w;
			
			n = n->previous;
		}
		
		n = lower;
		if ( lower == greater && n != NULL && !n->is_absent ) {
			// Conto il punto i
			
			double d = 0.0;
			double w = exp( - abs(d) / (2*sigma*sigma) );
			
			smoothed += n->blob.center * w;
			total_weight += w;
		}
		
		if ( total_weight > 0.0 ) smoothed *= 1.0 / total_weight;
		
		// End smoothing
		
		if ( lower == NULL || greater == NULL ) {
			logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
					"no ball found (path does not pass through current frame - lower == NULL || greater == NULL)" << endl;
			positions.push_back(NISBA);
			continue;
		}
		
		// Non dovrebbe succedere: un cammino migliore si sarebbe ottenuto passando dal nodo "absent" di questo frame
		assert( greater == lower || (!greater->is_absent && !lower->is_absent) );

		if ( greater->time == lower->time ) {
			// Il fotogramma in questione non e' stato saltato
			
			if ( greater->is_absent ) {
				// Nel fotogramma in questione la pallina e' stata valutata assente
				logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
						"no ball found (ball claimed to be absent)" << endl;
				positions.push_back(NISBA);
				continue;
			}
			else {
				// Nel fotogramma in questione la pallina e' stata valutata presente
				// position = greater->blob.center;
				position = smoothed;	// Metto il valore smoothed
				logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
						"ball found (exact location: " << position << ")" << endl;
				positions.push_back( Point2f(position) );
				continue;
			}
		}
		
		
		int pre_diff = i - lower->time;
		int post_diff = greater->time - i;
	
		if ( (double)(max( pre_diff, post_diff )) > _max_interpolation_time * _fps ) {
			// Se è richiesta la posizione in un frame molto lontano da quelli in cui passa il percorso, restituisco NISBA.
			logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
					"no ball found (interpolation not reliable)" << endl;
			positions.push_back(NISBA);
			continue;
		}
		
		cv::Point2f greater_center = greater->blob.center;
		cv::Point2f lower_center = lower->blob.center;
	
		// position = ( greater_center*(i - lower->time) + lower_center*(greater->time - i) ) * ( 1.0/(greater->time - lower->time) );
		position = smoothed;	// Metto il valore smoothed
		logger(panel, "ball tracking", VERBOSE) << "Frame " << i <<
				"ball found (estimated location: " << position << ")" << endl;
		positions.push_back( Point2f(position) );
	}
	
	return positions;
}


/* OLD VERSION

Point2f BlobsTracker::ProcessFrame(int initial_time, int processed_time) {
	
	printf("Processing frame %d\n", processed_time);
	
	 // TODO: aggiustare questa min_badness, che serve a evitare falsi positivi quando non c'è la pallina vera
	double min_badness = (double)(_timeline.size()) / 1.0;
	printf("Maximum acceptable badness: %.1lf\n", min_badness);
	Node *best_node = NULL;
	
	for (int i=0; i<_timeline.size(); i++) {
		for (int k=0; k<_timeline[i].size(); k++) {
			
			// Passo base (collegamenti con il nodo fittizio iniziale)
			_timeline[i][k].badness = i;
			_timeline[i][k].previous = NULL;
			
			// _timeline[i][k].subnodes.clear();
			
			
			// Programmazione dinamica
			for (int j=0; j<i; j++) {
				for (int h=0; h<_timeline[j].size(); h++) {
					
					int interval = i-j;
					double old_badness = _timeline[j][h].badness;
					double delta_badness = interval - 1;
					
					// Località spaziale --- forse Kalman la renderà obsoleta
					Point2f new_center = _timeline[i][k].blob.center;
					Point2f old_center = _timeline[j][h].blob.center;
					double distance = norm(new_center - old_center);
					
					// Controllo di località
					if ( distance > _max_speed * interval ) continue;
					// if ( distance > _max_unseen_distance ) continue;
					
					// Verosimiglianza gaussiana
					delta_badness += _distance_constant * distance * distance;
					
					
					// Calcolo la nuova badness, e aggiorno se è minore della minima finora trovata
					double new_badness = old_badness + delta_badness;
					
					if ( new_badness < _timeline[i][k].badness ) {
						_timeline[i][k].badness = new_badness;
						// Salvo il percorso ottimo
						_timeline[i][k].previous = &_timeline[j][h];
					}
					
				}
			}
			
			// Passo finale (collegamenti con il nodo fittizio finale)
			// TODO: controllare che questa cosa sia effettivamente al posto giusto
			double candidate_badness = _timeline[i][k].badness + ( _timeline.size() - i - 1 );
			if ( min_badness > candidate_badness ) {
				min_badness = candidate_badness;
				best_node = &_timeline[i][k];
			}
			
		}
	}
	
	
	Point2f NISBA (-1.0, -1.0);
	
	// Cerco la miglior posizione prevista per il frame richiesto
	Node *node = best_node;
	if ( node == NULL ) {
		printf("No ball found (no path).\n");
		return NISBA;
	}
	if ( node->time < processed_time ) {
		printf("No ball found (path does not pass through current frame).\n");
		return NISBA;
	}
	
	Node *greater = NULL;
	Node *lower = NULL;
	
	while ( node != NULL ) {
		int time = node->time;
		if ( time >= processed_time ) {
			greater = node;
		}
		if ( time <= processed_time ) {
			lower = node;
			break;
		}
		node = node->previous;
	}
	
	if ( lower == NULL ) {
		printf("No ball found (path does not pass through current frame).\n");
		return NISBA;
	}
	
	// Waiting for great Kalman (Hail, Kalman! Hail!)
	
	if ( greater->time == lower->time ) {
		printf("Ball found (exact location). Badness: %.1lf\n", min_badness);
		return greater->blob.center;
	}
	
	// Se è richiesta la posizione in un frame molto lontano da quelli in cui passa il percorso, restituisco NISBA.
	int pre_diff = processed_time - lower->time;
	int post_diff = greater->time - processed_time;
	
	if ( max( pre_diff, post_diff ) > _max_interpolation_time ) {
		printf("No ball found (interpolation is not reliable).\n");
		return NISBA;
	}
	
	
	cv::Point2f greater_center = greater->blob.center;
	cv::Point2f lower_center = lower->blob.center;
	
	printf("Ball found (estimated location). Badness: %.1lf\n", min_badness);
	return ( greater_center*(processed_time - lower->time) + lower_center*(greater->time - processed_time) ) * ( 1.0/(greater->time - lower->time) );
	
}
*/

void BlobsTracker::PopFrameFromTimeline() {
	_timeline.pop_front();
}

