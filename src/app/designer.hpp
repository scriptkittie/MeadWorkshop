// The Designer screen's model: holds the recipe being designed, applies edits from the screen,
// and hands back everything the screen shows (numbers, bars, options, review, nutrients, the math).
// All brewing logic lives in the Qt-free engine/advisor; this class only converts to and from Qt types.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <string>

#include "mead_advisor.hpp"

class Designer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap design READ design NOTIFY changed)
    Q_PROPERTY(QVariantMap results READ results NOTIFY changed)
    Q_PROPERTY(QString sweetnessScale READ sweetnessScale WRITE setSweetnessScale NOTIFY changed)
    Q_PROPERTY(double honeyPPG READ honeyPPG WRITE setHoneyPPG NOTIFY changed)
    Q_PROPERTY(bool holdOG READ holdOG WRITE setHoldOG NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QVariantMap goal READ goal NOTIFY goalChanged)
    Q_PROPERTY(QVariantMap plan READ planView NOTIFY planChanged)
    Q_PROPERTY(double fermenterGal READ fermenterGal WRITE setFermenterGal NOTIFY changed)
    Q_PROPERTY(QVariantList yeastChoices READ yeastChoices NOTIFY goalChanged)
    Q_PROPERTY(QString savedAs READ savedAs NOTIFY changed)      // the saved recipe's file name (no .json), "" = not saved yet
    Q_PROPERTY(bool unsaved READ unsaved NOTIFY changed)         // changes since the last save (or a recipe never saved)
    Q_PROPERTY(QString recipesFolder READ recipesFolder CONSTANT)

public:
    explicit Designer(QObject *parent = nullptr);

    QVariantMap design() const { return m_designMap; }
    QVariantMap results() const { return m_results; }
    QString sweetnessScale() const { return QString::fromStdString(m_scale); }
    void setSweetnessScale(const QString &s);
    double honeyPPG() const { return m_settings.honeyPPG; }
    void setHoneyPPG(double v);
    bool holdOG() const { return m_holdOG; }
    void setHoldOG(bool v);
    QString message() const { return m_message; }
    QVariantMap goal() const { return m_goalMap; }
    QVariantMap planView() const { return m_planView; }
    double fermenterGal() const { return m_fermenterGal; }
    QVariantList yeastChoices() const { return m_yeastChoices; }   // Plan's Yeast list, marked ★ ✓ * ✗ ? for the goal
    void setFermenterGal(double v);
    QString savedAs() const { return m_savedAs; }
    bool unsaved() const { return m_unsaved; }
    QString recipesFolder() const;   // Documents/Mead Workshop/My recipes

    // ---- reference lists for the drop-downs ----
    Q_INVOKABLE QVariantList fruitOptions() const;
    Q_INVOKABLE QVariantList juiceOptions() const;
    Q_INVOKABLE QVariantList yeastOptions() const;
    Q_INVOKABLE QStringList honeyNames() const;
    Q_INVOKABLE QStringList sugarNames() const;
    Q_INVOKABLE QStringList adjunctNames() const;
    Q_INVOKABLE QVariantList styleChoices() const;   // "Auto (from ingredients)" + the styles in Piatz ch.2
    Q_INVOKABLE QVariantList publishedRecipes() const;
    Q_INVOKABLE QVariantList sweetnessChoices() const;

    // ---- edits ----
    // path like "name", "volumeGal", "honeys.0.lb", "fruits.1.fruitId" (same paths as the browser version)
    Q_INVOKABLE void setField(const QString &path, const QVariant &value);
    Q_INVOKABLE void addRow(const QString &list);
    Q_INVOKABLE void removeRow(const QString &list, int index);
    Q_INVOKABLE void newDesign();
    Q_INVOKABLE void adaptPublished(const QString &recipeId, double volumeGal);
    Q_INVOKABLE void solveForOG(const QString &og);
    Q_INVOKABLE void solveForABV(const QString &abv);

    // ---- My recipes (one .json file per recipe in recipesFolder) ----
    Q_INVOKABLE void saveRecipe();                         // save under its name (renames the file if the name changed)
    Q_INVOKABLE void saveRecipeAs(const QString &name);    // a new copy with this name; the old file is kept
    Q_INVOKABLE QVariantList myRecipes() const;            // {file, name, style, volumeGal, saved, kind}, newest first
    Q_INVOKABLE void openRecipe(const QString &file);
    Q_INVOKABLE void deleteRecipe(const QString &file);    // to the Recycle Bin (Windows) or Trash (Linux)
    Q_INVOKABLE void showRecipesFolder() const;

    // ---- profile bars ----
    Q_INVOKABLE QVariantList previewSweetOptions(double targetFG) const;   // while the slider moves
    Q_INVOKABLE void setTargetFG(double targetFG);                          // slider released
    Q_INVOKABLE void applyOption(double og, double fg);                     // option A / "less honey"
    Q_INVOKABLE void addBacksweetening(double lb, double fg);               // option B
    Q_INVOKABLE QString previewFruitLevel(int index, double lb) const;
    Q_INVOKABLE void setFruitAmount(int index, double lb);

    // ---- goal planner ("I have 20 lb of pears…") ----
    Q_INVOKABLE void setGoalField(const QString &path, const QVariant &value);  // "abv", "fruits.0.haveLb", …
    Q_INVOKABLE void addGoalFruit();
    Q_INVOKABLE void removeGoalFruit(int index);
    Q_INVOKABLE void addGoalSpice();
    Q_INVOKABLE void removeGoalSpice(int index);
    Q_INVOKABLE QVariantList spiceOptions() const;                         // spices, herbs, flowers, oak: {id, name}
    Q_INVOKABLE QString spiceGuide(const QString &name, double gal) const; // subtle / noticeable / strong for this batch, or ""
    Q_INVOKABLE void runPlan();
    Q_INVOKABLE void usePlan();          // copy the plan into the Designer
    Q_INVOKABLE void useYeast(const QString &yeastId, double abv);   // choose a yeast (abv > 0 also sets the target) and re-plan

    // ---- word list (everyday words, with the brewing term) ----
    Q_INVOKABLE QString linked(const QString &text) const;          // the sentence with tappable terms (Qt StyledText)
    Q_INVOKABLE QVariantList glossary() const;                       // every entry: key, term, plain, group, what, src
    Q_INVOKABLE QStringList glossaryGroups() const;
    Q_INVOKABLE QVariantMap term(const QString &key) const;
    Q_INVOKABLE void explainTerm(const QString &linkOrKey);          // "term:og" or "og" → termRequested("og")

signals:
    void changed();
    void messageChanged();
    void goalChanged();
    void planChanged();
    void openDesigner();
    void termRequested(const QString &key);

private:
    void commit();             // recompute, save the draft, notify
    void say(const QString &msg);
    void loadSettings();
    void saveSettings() const;
    void loadDraft();
    void saveDraft() const;
    bool writeRecipe(const QString &stem);
    void forgetSaved();          // the recipe on screen isn't a saved file (New, a book recipe, a plan)
    QString m_savedAs;
    std::string m_savedText;     // the recipe as last saved/opened, to tell whether there are unsaved changes
    bool m_unsaved = false;
    QVariantMap toMap(const mead::Design &d) const;
    QVariantMap compute() const;
    const mead::json::Value *publishedById(const std::string &id) const;
    void loadBookPack();
    QString m_bookPack;   // path of the book pack in use, if any

    mead::Database m_db;
    mead::json::Value m_recipes;    // published recipes (recipes.json)
    mead::Design m_design;
    mead::Goal m_goal;
    QVariantMap m_goalMap, m_planView;
    QVariantList m_yeastChoices;
    void goalCommitted();
    void loadGoal();
    void saveGoal() const;
    mead::Settings m_settings;
    std::string m_scale = "piatz";
    bool m_holdOG = true;
    double m_fermenterGal = 7.2;   // your primary fermenter (0 = don't check)
    QVariantMap m_designMap, m_results;
    QString m_message;
};
