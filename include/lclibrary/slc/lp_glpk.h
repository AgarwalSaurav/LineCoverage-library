/**
 * This file is part of the LineCoverage-library.
 * The file contains SLC LP solver using GLPK
 *
 * TODO:
 *
 * @author Saurav Agarwal
 * @contact sagarw10@uncc.edu
 * @contact agr.saurav1@gmail.com
 * Repository: https://github.com/UNCCharlotte-Robotics/LineCoverage-library
 *
 * Copyright (C) 2020--2022 University of North Carolina at Charlotte.
 * The LineCoverage-library is owned by the University of North Carolina at Charlotte and is protected by United States copyright laws and applicable international treaties and/or conventions.
 *
 * The LineCoverage-library is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * DISCLAIMER OF WARRANTIES: THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT WARRANTY OF ANY KIND INCLUDING ANY WARRANTIES OF PERFORMANCE OR MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR PURPOSE OR OF NON-INFRINGEMENT. YOU BEAR ALL RISK RELATING TO QUALITY AND PERFORMANCE OF THE SOFTWARE OR HARDWARE.
 *
 * SUPPORT AND MAINTENANCE: No support, installation, or training is provided.
 *
 * You should have received a copy of the GNU General Public License along with LineCoverage-library. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LCLIBRARY_SLC_LP_GLPK_H_
#define LCLIBRARY_SLC_LP_GLPK_H_

#include <glpk.h>
#include <cmath>
#include <memory>
#include <lclibrary/core/constants.h>
#include <lclibrary/core/graph.h>
#include <lclibrary/slc/slc_lp.h>

namespace lclibrary {

	class SLC_LP_glpk : public SLC_LP {
		size_t n_, m_, m_nr_;
		size_t num_of_variables_;
		size_t num_of_constraints_;
		int constraint_count_;
		std::vector <int> vec_ia;
		std::vector <int> vec_ja;
		std::vector <double> vec_ar;
		int *ia_, *ja_;
		double *ar_;
		size_t coeff_count_;
		size_t number_of_coeff_;
		size_t depot_index_;
		double z_;

		glp_prob * lp_;
		std::shared_ptr <const Graph> g_;

		public:
		SLC_LP_glpk(const std::shared_ptr <const Graph> );
		~SLC_LP_glpk() {
			glp_delete_prob(lp_);
			delete [] ia_;
			delete [] ja_;
			delete [] ar_;
		}

		bool NearZero(double x) {
			if (std::abs(x) < 1e-10)
				return true;
			else
				return false;
		}
		void AddVars();
		void AddRows();
		void SymmetryConstraints();
		void ServiceConstraints();
		void AddConstraintElement(int, int, double);
		void TourCost();
		void LoadConstraintMatrix();
		int Solve();
		void PrintSolution(const char *fname) {
			glp_write_mip(lp_, fname);
		}
		int GenerateSolutionGraph(std::shared_ptr <Graph> &, std::shared_ptr <Graph> &);

	};

}
#endif /* LCLIBRARY_SLC_LP_GLPK_H_ */
