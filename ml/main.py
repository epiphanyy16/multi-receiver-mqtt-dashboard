
import pandas as pd
from sklearn.preprocessing import StandardScaler
from sklearn.neighbors import KNeighborsClassifier
from sklearn.tree import DecisionTreeClassifier
from sklearn.svm import SVC
from sklearn.model_selection import cross_val_score
from sklearn.model_selection import cross_val_predict
from sklearn.model_selection import StratifiedKFold
from sklearn.model_selection import GridSearchCV
from sklearn.linear_model import LogisticRegression
from sklearn.neural_network import MLPClassifier
from sklearn.metrics import classification_report
from sklearn.metrics import confusion_matrix


rssi = pd.read_excel("RSSI_Vectors.xlsx")
labels = pd.read_excel("Labels.xlsx")

rssi_filled=rssi.fillna(-110)

scalar=StandardScaler()
rssi_scaled=scalar.fit_transform(rssi_filled)

model = SVC(kernel="poly", degree=2, C=10, coef0=1)

cv_strategy = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
'''scores = cross_val_score(model, rssi_scaled, labels["Room"], cv=cv_strategy)

print(labels["Room"])
print("Fold accuracies:", scores)
print("Mean accuracy:", scores.mean())


param_grid = {
    "C": [0.1, 1, 10, 100],
    "coef0": [0, 1, 5],
}

model = SVC(kernel="poly", degree=2)
cv_strategy = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
grid = GridSearchCV(model, param_grid, cv=cv_strategy)
grid.fit(rssi_scaled, labels["Room"])

print("Best params:", grid.best_params_)
print("Best cross-validated accuracy:", grid.best_score_)'''

y_pred = cross_val_predict(model, rssi_scaled, labels["Room"], cv=cv_strategy)
print(classification_report(labels["Room"], y_pred))

room_labels = sorted(labels["Room"].unique())
cm = confusion_matrix(labels["Room"], y_pred, labels=room_labels)

cm_df = pd.DataFrame(cm, index=room_labels, columns=room_labels)
print(cm_df)