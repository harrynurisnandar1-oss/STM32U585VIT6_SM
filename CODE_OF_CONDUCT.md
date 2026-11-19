# Contributor Code of Conduct
## 3-Phase AMI Smart Meter Project (STM32U585VIT6 + ADE9000)

---

## 1. Our Pledge

As contributors, developers, and maintainers of the **3-Phase AMI Smart Meter (STM32U585VIT6 + ADE9000)** project, we are committed to fostering an open, safe, professional, inclusive, and harassment-free collaboration environment for everyone, regardless of age, sex, gender identity, sexual orientation, disability, physical appearance, race, ethnicity, religion, or level of technical experience.

---

## 2. Expected Standards of Behavior

Behaviors that contribute to a healthy and productive engineering environment include:

* **Respectful & Open Communication**: Using polite, professional, and empathetic language in all issue discussions, code reviews, and team meetings.
* **Focus on Technical Fact & Safety**: Prioritizing engineering quality, high-voltage electrical lab safety, metrology standards compliance (SPLN / IEC), and cybersecurity reliability.
* **Gracefully Accepting Constructive Feedback**: Remaining open to constructive technical criticism aimed at improving system architecture and reliability.
* **Ego-Free Collaboration**: Focusing on team success, sharing knowledge, helping teammates overcome blockers, and respecting every discipline (Hardware, Firmware, QA, Metrology).

---

## 3. Unacceptable Behavior

The following behaviors are considered violations and are strictly prohibited in this project:

* Use of sexualized, derogatory, or discriminatory language, imagery, or content.
* Trolling, verbal harassment, insulting/derogatory comments, and personal attacks (*ad hominem*).
* Public or private harassment of any form.
* Publishing others' private information (such as phone numbers, home addresses, or confidential credentials) without explicit permission (*doxxing*).
* Reckless actions regarding electrical lab safety, dishonest manipulation of metrology test data, or intentional compromise of system integrity.
* Any other conduct reasonably considered unprofessional in an engineering context.

---

## 4. Safety & Integrity Ethics for Critical Systems

Because this project involves high-voltage 3-phase electrical hardware ($230/400\text{ VAC}$) and critical utility billing systems:

1. **Metrology Integrity**: Intentionally introducing power/energy measurement manipulation logic that violates legal metrology standards (SPLN D3.006 / IEC 62053) is strictly forbidden.
2. **Cybersecurity Compliance (*Fail-Closed*)**: Every contribution affecting cryptography, TrustZone, and key management must comply with the SPLN D5.002:2024 security architecture and must never weaken security posture for shortcuts (*no hardcoded keys, no bypass*).
3. **Laboratory Safety**: Every high-voltage experiment must follow the *two-person rule* and comply with *Safety Gate* authorization.

---

## 5. Maintainer Responsibilities

Project maintainers are responsible for clarifying and enforcing acceptable behavior standards, and taking fair, proportionate corrective action in response to any violation of this Code of Conduct.

Maintainers have the right and responsibility to remove, edit, or reject comments, commits, code, pull requests, or issues that are not aligned with this Code of Conduct.

---

## 6. Reporting Violations & Enforcement

Instances of inappropriate behavior, intimidation, or harassment may be reported confidentially to the program lead or project coordinator. All reports will be reviewed fairly, confidentially, and promptly.

Enforcement actions may include:
1. **Warning / Clarification**: A private written warning regarding behavior that violates professional standards.
2. **Corrective Action**: Reversion or rejection of the associated contribution.
3. **Temporary Restriction**: Temporary suspension of access to the repository or project discussion channels.
4. **Permanent Removal**: Revocation of access rights and permanent ban from all project activities.

---

## 7. Attribution & References

This Code of Conduct is adapted from the **Contributor Covenant**, version 2.1 (https://www.contributor-covenant.org/version/2/1/code_of_conduct.html), tailored for electrical utility embedded hardware and firmware engineering.
