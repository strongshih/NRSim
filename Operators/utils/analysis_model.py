# analysis/analysis_model.py

import pandas as pd

def analysis_model(model_operators, system):
    roofline_list = []
    for operator_instance in model_operators:
        roofline = operator_instance.get_roofline(system=system)
        roofline_list.append(roofline)
    df = pd.DataFrame(roofline_list)
    return df
